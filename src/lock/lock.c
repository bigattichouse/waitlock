/*
 * Lock management functions - lock acquisition, release, checking, listing
 */

#include "lock.h"
#include "../core/core.h"
#include "../process/process.h"
#include "../checksum/checksum.h"

/* Find or create lock directory */
char* find_lock_directory(void) {
    static char lock_dir[PATH_MAX];
    const char *candidates[] = {
        "/var/run/waitlock",
        "/run/waitlock",
        "/var/lock/waitlock",
        "/tmp/waitlock",
        NULL
    };
    const char **p;
    char *home;
    
    /* Use specified directory if provided */
    if (opts.lock_dir) {
        if (access(opts.lock_dir, W_OK) == 0) {
            return (char*)opts.lock_dir;
        }
        if (errno == ENOENT) {
            if (mkdir(opts.lock_dir, 0755) == 0) {
                return (char*)opts.lock_dir;
            }
        }
        return NULL;
    }
    
    /* Try standard locations */
    for (p = candidates; *p; p++) {
        if (access(*p, W_OK) == 0) {
            return (char*)*p;
        }
        if (errno == ENOENT) {
            if (mkdir(*p, 0755) == 0) {
                return (char*)*p;
            }
        }
    }
    
    /* Try home directory */
    home = getenv("HOME");
    if (home) {
        safe_snprintf(lock_dir, sizeof(lock_dir), "%s/.waitlock", home);
        if (access(lock_dir, W_OK) == 0) {
            return lock_dir;
        }
        if (errno == ENOENT) {
            if (mkdir(lock_dir, 0755) == 0) {
                return lock_dir;
            }
        }
    }
    
    /* Last resort: current directory */
    safe_snprintf(lock_dir, sizeof(lock_dir), "./waitlock");
    if (access(lock_dir, W_OK) == 0) {
        return lock_dir;
    }
    if (errno == ENOENT) {
        if (mkdir(lock_dir, 0755) == 0) {
            return lock_dir;
        }
    }
    
    return NULL;
}

/* Portable file locking */
int portable_lock(int fd, int operation) {
#ifdef HAVE_FLOCK
    return flock(fd, operation);
#else
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = (operation & LOCK_EX) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    
    int cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;
    return fcntl(fd, cmd, &fl);
#endif
}


/* Acquire lock */
int acquire_lock(const char *descriptor, int max_holders, double timeout) {
    char *lock_dir;
    char lock_path[PATH_MAX];
    char temp_path[PATH_MAX];
    char hostname[MAX_HOSTNAME];
    struct lock_info info;
    DIR *dir;
    struct dirent *entry;
    int fd;
    struct timeval start_time, now;
    double elapsed;
    int wait_ms = INITIAL_WAIT_MS;
    bool contention_logged = FALSE;
    
    /* Find lock directory */
    debug("DEBUG: Finding lock directory...");
    lock_dir = find_lock_directory();
    if (!lock_dir) {
        error(E_NODIR, "Cannot find or create lock directory (tried: %s, %s, %s, %s)", 
              "/var/lock/waitlock", "/tmp/waitlock", "/tmp", "./waitlock");
        return E_NODIR;
    }
    debug("DEBUG: Lock directory found: %s", lock_dir);
    
    /* Get hostname */
    debug("DEBUG: Getting hostname...");
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        safe_snprintf(hostname, sizeof(hostname), "unknown");
    }
    hostname[sizeof(hostname) - 1] = '\0';
    debug("DEBUG: Hostname: %s", hostname);
    
    /* Prepare lock info */
    memset(&info, 0, sizeof(info));
    info.magic = LOCK_MAGIC;
    info.version = 1;
    info.pid = getpid();
    info.ppid = getppid();
    info.uid = getuid();
    info.lock_type = (max_holders > 1) ? 1 : 0;
    info.max_holders = max_holders;
    info.slot = 0;  /* Will be set when slot is assigned */
    info.reserved = 0;
    strncpy(info.hostname, hostname, sizeof(info.hostname) - 1);
    info.hostname[sizeof(info.hostname) - 1] = '\0';
    strncpy(info.descriptor, descriptor, sizeof(info.descriptor) - 1);
    info.descriptor[sizeof(info.descriptor) - 1] = '\0';
    
    /* Get command line */
    debug("DEBUG: Getting command line...");
    char *cmdline = get_process_cmdline(info.pid);
    if (cmdline) {
        strncpy(info.cmdline, cmdline, sizeof(info.cmdline) - 1);
    }
    debug("DEBUG: Command line obtained");
    
    /* Try to acquire lock */
    debug("DEBUG: Starting lock acquisition...");
    gettimeofday(&start_time, NULL);
    
    while (1) {
        /* Check timeout at start of each iteration to prevent hanging */
        if (timeout >= 0) {
            gettimeofday(&now, NULL);
            elapsed = (now.tv_sec - start_time.tv_sec) + 
                     (now.tv_usec - start_time.tv_usec) / 1000000.0;
            if (elapsed >= timeout) {
                /* Log timeout to syslog if requested */
                if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                    openlog("waitlock", LOG_PID, g_state.syslog_facility);
                    syslog(LOG_WARNING, "timeout waiting for lock '%s' after %.1f seconds", 
                           descriptor, timeout);
                    closelog();
#endif
                }
                error(E_TIMEOUT, "Timeout waiting for lock '%s' after %.1f seconds", descriptor, timeout);
                return E_TIMEOUT;
            }
        }
        
        /* Clean up stale locks during slot attempt - no separate counting phase */
        debug("DEBUG: Attempting atomic slot claiming (no pre-counting)...");
        
        /* Try to claim an available slot atomically */
        int slot_claimed = -1;
        int active_locks_found = 0; /* Count during slot attempts for debugging */
        
        /* Always try to claim a slot - let atomic operations determine availability */
        int start_slot = (opts.preferred_slot >= 0 && opts.preferred_slot < max_holders) ? opts.preferred_slot : 0;
        int slot_attempt, try_slot;
        
        debug("DEBUG: Starting slot claiming process...");
        
        /* Try each slot in sequence, starting with preferred slot */
        for (slot_attempt = 0; slot_attempt < max_holders; slot_attempt++) {
            try_slot = (start_slot + slot_attempt) % max_holders;
            
            debug("DEBUG: Trying slot %d (attempt %d)", try_slot, slot_attempt);
            
            /* First, check if this slot already exists and clean up if stale */
            safe_snprintf(lock_path, sizeof(lock_path), "%s/%s.slot%d.lock",
                          lock_dir, descriptor, try_slot);
            
            /* Check existing lock file for this slot */
            struct lock_info existing_info;
            bool slot_is_free = TRUE;
            
            if (read_lock_file_any_format(lock_path, &existing_info) == 0) {
                debug("DEBUG: Found existing lock file for slot %d", try_slot);
                if (existing_info.magic == LOCK_MAGIC && validate_lock_checksum(&existing_info)) {
                    if (process_exists(existing_info.pid)) {
                        debug("DEBUG: Slot %d is held by active process %d", try_slot, existing_info.pid);
                        slot_is_free = FALSE;
                        active_locks_found++;
                    } else {
                        debug("DEBUG: Slot %d is stale (process %d dead), cleaning up", try_slot, existing_info.pid);
                        unlink(lock_path);
                        /* Log stale lock cleanup */
                        if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                            openlog("waitlock", LOG_PID, g_state.syslog_facility);
                            syslog(LOG_INFO, "removed stale lock slot %d (pid %d no longer exists)", 
                                   try_slot, existing_info.pid);
                            closelog();
#endif
                        }
                    }
                } else {
                    debug("DEBUG: Slot %d has corrupted lock file, cleaning up", try_slot);
                    unlink(lock_path);
                }
            } else {
                debug("DEBUG: Slot %d is free (no existing lock file)", try_slot);
            }
            
            /* If slot is free, try to claim it atomically */
            if (slot_is_free) {
                /* Set slot in lock info */
                info.slot = try_slot;
                
                /* Create temp file for atomic claiming */
                safe_snprintf(temp_path, sizeof(temp_path), "%s/.tmp.%d.%d",
                              lock_dir, (int)info.pid, rand());
                
                info.acquired_at = time(NULL);
                /* Calculate and set checksum before writing */
                info.checksum = calculate_lock_checksum(&info);
                
                debug("DEBUG: Creating temp file: %s", temp_path);
                
                /* Try binary format first */
                fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
                if (fd >= 0) {
                    debug("DEBUG: Temp file created successfully");
                    if (write(fd, &info, sizeof(info)) == sizeof(info)) {
                        close(fd);
                        debug("DEBUG: Binary format written successfully");
                    } else {
                        /* Binary write failed, try text fallback */
                        close(fd);
                        unlink(temp_path);
                        debug("DEBUG: Binary write failed, trying text fallback");
                        if (write_text_lock_file(temp_path, &info) != 0) {
                            error(E_SYSTEM, "Cannot write lock info: %s", strerror(errno));
                            return E_SYSTEM;
                        }
                        debug("Used text fallback format for lock file");
                    }
                } else {
                    debug("DEBUG: Failed to create temp file: %s", strerror(errno));
                    error(E_SYSTEM, "Cannot create temp file: %s", strerror(errno));
                    return E_SYSTEM;
                }
                
                debug("DEBUG: Attempting atomic rename: %s -> %s", temp_path, lock_path);
                
                /* Atomic rename - this is where we claim the slot */
                if (rename(temp_path, lock_path) == 0) {
                    /* Successfully claimed this slot */
                    debug("DEBUG: Slot %d claimed successfully", try_slot);
                    slot_claimed = try_slot;
                    break;
                } else {
                    /* Slot was claimed by another process between our check and rename */
                    debug("DEBUG: Rename failed: %s (errno=%d) - slot was claimed by another process", strerror(errno), errno);
                    unlink(temp_path);
                    if (errno != EEXIST) {
                        /* Real error, not just slot collision */
                        error(E_SYSTEM, "Cannot create lock file: %s", strerror(errno));
                        return E_SYSTEM;
                    }
                    debug("Slot %d was claimed by another process, trying next slot", try_slot);
                    active_locks_found++; /* Count this as an active lock now */
                }
            }
        }
        
        debug("DEBUG: Slot claiming complete. Found %d active locks, claimed slot: %d", 
              active_locks_found, slot_claimed);
        
        /* Check if we've exceeded the maximum holders */
        if (slot_claimed < 0 && active_locks_found >= max_holders) {
            debug("DEBUG: All %d slots are occupied", max_holders);
            /* All slots occupied - continue to timeout check */
        } else if (slot_claimed < 0) {
            debug("DEBUG: Could not claim any slot despite availability - possible race condition");
            /* Retry after backoff */
        }
        
        if (slot_claimed >= 0) {
            /* Successfully claimed a slot */
            
            /* Apply flock for extra safety */
            g_state.lock_fd = open(lock_path, O_RDONLY);
            if (g_state.lock_fd >= 0) {
                portable_lock(g_state.lock_fd, LOCK_EX);
            }
            
            safe_snprintf(g_state.lock_path, sizeof(g_state.lock_path), "%s", lock_path);
            
            /* Log to syslog if requested */
            if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                openlog("waitlock", LOG_PID, g_state.syslog_facility);
                if (info.lock_type == 1) {
                    /* Semaphore */
                    if (opts.preferred_slot >= 0 && slot_claimed == opts.preferred_slot) {
                        syslog(LOG_INFO, "acquired lock '%s' slot %d (preferred) for '%s'", 
                               descriptor, slot_claimed, info.cmdline);
                    } else {
                        syslog(LOG_INFO, "acquired lock '%s' slot %d (auto) for '%s'", 
                               descriptor, slot_claimed, info.cmdline);
                    }
                } else {
                    /* Mutex */
                    syslog(LOG_INFO, "acquired lock '%s' for '%s'", 
                           descriptor, info.cmdline);
                }
                closelog();
#endif
            }
            
            debug("Lock acquired: %s (slot %d)", lock_path, slot_claimed);
            return E_SUCCESS;
        }
        
        /* No slot could be claimed - all slots are currently in use */
        debug("All %d slots are currently in use", max_holders);
        
        /* Check timeout again after slot claiming attempts */
        if (timeout >= 0) {
            gettimeofday(&now, NULL);
            elapsed = (now.tv_sec - start_time.tv_sec) + 
                     (now.tv_usec - start_time.tv_usec) / 1000000.0;
            if (elapsed >= timeout) {
                /* Log timeout to syslog if requested */
                if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                    openlog("waitlock", LOG_PID, g_state.syslog_facility);
                    syslog(LOG_WARNING, "timeout waiting for lock '%s' after %.1f seconds", 
                           descriptor, timeout);
                    closelog();
#endif
                }
                error(E_TIMEOUT, "Timeout waiting for lock '%s' after %.1f seconds", descriptor, timeout);
                return E_TIMEOUT;
            }
        }
        
        /* Check if we should exit */
        if (g_state.should_exit) {
            return E_SYSTEM;
        }
        
        /* Log contention on first wait */
        if (!contention_logged) {
            contention_logged = TRUE;
            
            /* Log lock contention to syslog with owner PID info */
            if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                openlog("waitlock", LOG_PID, g_state.syslog_facility);
                
                /* Find current lock holder to report in conflict message */
                DIR *dir = opendir(lock_dir);
                if (dir) {
                    struct dirent *entry;
                    pid_t holder_pid = 0;
                    
                    while ((entry = readdir(dir)) != NULL) {
                        if (strncmp(entry->d_name, descriptor, strlen(descriptor)) == 0 && 
                            strstr(entry->d_name, ".lock") != NULL) {
                            char lock_file_path[PATH_MAX];
                            struct lock_info check_info;
                            
                            safe_snprintf(lock_file_path, sizeof(lock_file_path), 
                                          "%s/%s", lock_dir, entry->d_name);
                            
                            if (read_lock_file_any_format(lock_file_path, &check_info) == 0 &&
                                process_exists(check_info.pid)) {
                                holder_pid = check_info.pid;
                                break;
                            }
                        }
                    }
                    closedir(dir);
                    
                    if (holder_pid > 0) {
                        syslog(LOG_INFO, "lock '%s' held by PID %d", 
                               descriptor, (int)holder_pid);
                    } else {
                        syslog(LOG_INFO, "lock contention for '%s' (waiting)", 
                               descriptor);
                    }
                } else {
                    syslog(LOG_INFO, "lock contention for '%s' (waiting)", 
                           descriptor);
                }
                closelog();
#endif
            }
        }
        
        /* Wait with exponential backoff */
        int sleep_ms = wait_ms;
        
        /* For timeouts, don't sleep longer than remaining time */
        if (timeout >= 0) {
            struct timeval now_check;
            gettimeofday(&now_check, NULL);
            double elapsed_check = (now_check.tv_sec - start_time.tv_sec) + 
                                  (now_check.tv_usec - start_time.tv_usec) / 1000000.0;
            double remaining = timeout - elapsed_check;
            if (remaining <= 0) {
                /* Already past timeout, return timeout error immediately */
                error(E_TIMEOUT, "Timeout waiting for lock '%s' after %.1f seconds", descriptor, timeout);
                return E_TIMEOUT;
            }
            /* Limit sleep to remaining timeout (with small margin) */
            int max_sleep_ms = (int)(remaining * 1000 * TIMEOUT_FACTOR); /* 90% of remaining */
            if (max_sleep_ms < sleep_ms) {
                sleep_ms = max_sleep_ms;
            }
            if (sleep_ms < 1) sleep_ms = 1; /* Minimum 1ms */
        }
        
        usleep(sleep_ms * 1000);
        wait_ms = wait_ms * 2;
        if (wait_ms > MAX_WAIT_MS) wait_ms = MAX_WAIT_MS;
        
        /* Add jitter */
        wait_ms += rand() % (wait_ms / 10 + 1);
    }
}

/* Release lock */
void release_lock(void) {
    if (g_state.lock_fd >= 0) {
        close(g_state.lock_fd);
        g_state.lock_fd = -1;
    }
    
    if (g_state.lock_path[0]) {
        /* Log to syslog if requested */
        if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
            openlog("waitlock", LOG_PID, g_state.syslog_facility);
            
            /* Extract descriptor name and calculate duration */
            char *descriptor_start = strrchr(g_state.lock_path, '/');
            if (descriptor_start) {
                descriptor_start++;  /* Skip the '/' */
                
                /* Find the end of descriptor name (before .slot) */
                char *descriptor_end = strstr(descriptor_start, ".slot");
                if (descriptor_end) {
                    char descriptor[MAX_DESC_LEN + 1];
                    size_t desc_len = descriptor_end - descriptor_start;
                    if (desc_len < sizeof(descriptor)) {
                        strncpy(descriptor, descriptor_start, desc_len);
                        descriptor[desc_len] = '\0';
                        
                        /* Read lock file to get acquisition time */
                        struct lock_info info;
                        if (read_lock_file_any_format(g_state.lock_path, &info) == 0) {
                            time_t now = time(NULL);
                            double duration = difftime(now, info.acquired_at);
                            syslog(LOG_INFO, "released lock '%s' after %.0f seconds", 
                                   descriptor, duration);
                        } else {
                            syslog(LOG_INFO, "released lock '%s'", descriptor);
                        }
                    } else {
                        syslog(LOG_INFO, "released lock: %s", g_state.lock_path);
                    }
                } else {
                    syslog(LOG_INFO, "released lock: %s", g_state.lock_path);
                }
            } else {
                syslog(LOG_INFO, "released lock: %s", g_state.lock_path);
            }
            closelog();
#endif
        }
        
        unlink(g_state.lock_path);
        debug("Lock released: %s", g_state.lock_path);
        g_state.lock_path[0] = '\0';
    }
}

/* Check if lock is available */
int check_lock(const char *descriptor) {
    char *lock_dir;
    DIR *dir;
    struct dirent *entry;
    int active_locks = 0;
    int max_holders = 1; /* Default to mutex behavior */
    
    lock_dir = find_lock_directory();
    if (!lock_dir) {
        return E_SYSTEM;
    }
    
    dir = opendir(lock_dir);
    if (!dir) {
        return E_SYSTEM;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Check if this is a lock file for our descriptor */
        char *dot = strchr(entry->d_name, '.');
        if (dot && strncmp(entry->d_name, descriptor, dot - entry->d_name) == 0 &&
            (size_t)(dot - entry->d_name) == strlen(descriptor) &&
            strstr(entry->d_name, ".lock")) {
            char check_path[PATH_MAX];
            struct lock_info info;
            
            safe_snprintf(check_path, sizeof(check_path), "%s/%s", 
                          lock_dir, entry->d_name);
            
            if (read_lock_file_any_format(check_path, &info) == 0) {
                if (info.magic == LOCK_MAGIC && validate_lock_checksum(&info) && process_exists(info.pid)) {
                    active_locks++;
                    /* Use max_holders from any valid lock file */
                    max_holders = info.max_holders;
                } else if (info.magic == LOCK_MAGIC && !validate_lock_checksum(&info)) {
                    /* Corrupted lock file - clean it up */
                    debug("Removing corrupted lock file: %s", entry->d_name);
                    unlink(check_path);
                    
                    /* Log corrupted lock cleanup to syslog */
                    if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                        openlog("waitlock", LOG_PID, g_state.syslog_facility);
                        syslog(LOG_WARNING, "removed corrupted lock file: %s (invalid checksum)", 
                               entry->d_name);
                        closelog();
#endif
                    }
                }
            }
        }
    }
    
    closedir(dir);
    
    /* Log check operation result to syslog */
    if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
        openlog("waitlock", LOG_PID, g_state.syslog_facility);
        syslog(LOG_INFO, "check lock '%s': %s (%d/%d holders)", 
               descriptor, (active_locks >= max_holders) ? "busy" : "available", 
               active_locks, max_holders);
        closelog();
#endif
    }
    
    return (active_locks >= max_holders) ? E_BUSY : E_SUCCESS;
}

/* List locks */
int list_locks(output_format_t format, bool show_all, bool stale_only) {
    char *lock_dir;
    DIR *dir;
    struct dirent *entry;
    
    lock_dir = find_lock_directory();
    if (!lock_dir) {
        error(E_NODIR, "Cannot find lock directory for listing locks");
        return E_NODIR;
    }
    
    dir = opendir(lock_dir);
    if (!dir) {
        error(E_SYSTEM, "Cannot open lock directory '%s': %s", lock_dir, strerror(errno));
        return E_SYSTEM;
    }
    
    /* Print header */
    if (format == FMT_HUMAN && !g_state.quiet) {
        printf("%-18s %-6s %-4s %-8s %-19s %s\n",
               "DESCRIPTOR", "PID", "SLOT", "USER", "ACQUIRED", "COMMAND");
    } else if (format == FMT_CSV && !g_state.quiet) {
        printf("descriptor,pid,slot,user,acquired,status,command\n");
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".lock")) {
            char lock_path[PATH_MAX];
            struct lock_info info;
            bool is_stale = FALSE;
            
            safe_snprintf(lock_path, sizeof(lock_path), "%s/%s", 
                          lock_dir, entry->d_name);
            
            if (read_lock_file_any_format(lock_path, &info) != 0) {
                continue;  /* Cannot read lock file */
            }
            
            if (info.magic != LOCK_MAGIC) continue;
            
            /* Validate checksum - skip corrupted files */
            if (!validate_lock_checksum(&info)) {
                debug("Skipping corrupted lock file: %s", entry->d_name);
                continue;
            }
            
            is_stale = !process_exists(info.pid);
            
            if (stale_only && !is_stale) continue;
            if (!show_all && is_stale) continue;
            
            /* Get user info */
            struct passwd *pw = getpwuid(info.uid);
            const char *username = pw ? pw->pw_name : "unknown";
            
            /* Format time */
            char time_str[20];
            struct tm *tm = localtime(&info.acquired_at);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
            
            /* Output based on format */
            if (format == FMT_HUMAN) {
                if (is_stale) {
                    printf("  [STALE]          (%-4d) %-4s %-8s %-19s %s\n",
                           (int)info.pid, info.lock_type == 1 ? "n/a" : "-", username, time_str, 
                           info.cmdline[0] ? info.cmdline : "Process no longer exists");
                } else {
                    if (info.lock_type == 1) {
                        /* Semaphore - show slot */
                        printf("%-18s %-6d %-4d %-8s %-19s %s\n",
                               info.descriptor, (int)info.pid, info.slot, username, time_str, info.cmdline);
                    } else {
                        /* Mutex - no slot */
                        printf("%-18s %-6d %-4s %-8s %-19s %s\n",
                               info.descriptor, (int)info.pid, "-", username, time_str, info.cmdline);
                    }
                }
            } else if (format == FMT_CSV) {
                printf("%s,%d,%d,%s,%ld,%s,%s\n",
                       info.descriptor, (int)info.pid, info.slot, username, 
                       (long)info.acquired_at, is_stale ? "stale" : "active", 
                       info.cmdline);
            } else if (format == FMT_NULL) {
                printf("%s%c%d%c%d%c%s%c%ld%c%s%c%s%c%c",
                       info.descriptor, '\0', (int)info.pid, '\0', info.slot, '\0', username, '\0',
                       (long)info.acquired_at, '\0', is_stale ? "stale" : "active", '\0',
                       info.cmdline, '\0', '\0');
            }
        }
    }
    
    closedir(dir);
    return E_SUCCESS;
}

/* Write lock information in text format as fallback */
int write_text_lock_file(const char *path, const struct lock_info *info) {
    FILE *fp;
    
    fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }
    
    fprintf(fp, "VERSION=%u\n", info->version);
    fprintf(fp, "PID=%d\n", (int)info->pid);
    fprintf(fp, "PPID=%d\n", (int)info->ppid);
    fprintf(fp, "UID=%d\n", (int)info->uid);
    fprintf(fp, "ACQUIRED=%ld\n", (long)info->acquired_at);
    fprintf(fp, "TYPE=%s\n", info->lock_type ? "semaphore" : "mutex");
    fprintf(fp, "MAX_HOLDERS=%d\n", info->max_holders);
    fprintf(fp, "SLOT=%d\n", info->slot);
    fprintf(fp, "HOSTNAME=%s\n", info->hostname);
    fprintf(fp, "DESCRIPTOR=%s\n", info->descriptor);
    fprintf(fp, "COMMAND=%s\n", info->cmdline);
    
    fclose(fp);
    return 0;
}

/* Read lock information from text format */
int read_text_lock_file(const char *path, struct lock_info *info) {
    FILE *fp;
    char line[1024];
    char *equals;
    char type_str[32];
    
    memset(info, 0, sizeof(*info));
    info->magic = LOCK_MAGIC;  /* Set magic for text format too */
    
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        equals++;
        
        if (strcmp(line, "VERSION") == 0) {
            info->version = atoi(equals);
        } else if (strcmp(line, "PID") == 0) {
            info->pid = atoi(equals);
        } else if (strcmp(line, "PPID") == 0) {
            info->ppid = atoi(equals);
        } else if (strcmp(line, "UID") == 0) {
            info->uid = atoi(equals);
        } else if (strcmp(line, "ACQUIRED") == 0) {
            info->acquired_at = atol(equals);
        } else if (strcmp(line, "TYPE") == 0) {
            strncpy(type_str, equals, sizeof(type_str) - 1);
            type_str[sizeof(type_str) - 1] = '\0';
            info->lock_type = (strcmp(type_str, "semaphore") == 0) ? 1 : 0;
        } else if (strcmp(line, "MAX_HOLDERS") == 0) {
            info->max_holders = atoi(equals);
        } else if (strcmp(line, "SLOT") == 0) {
            info->slot = atoi(equals);
        } else if (strcmp(line, "HOSTNAME") == 0) {
            strncpy(info->hostname, equals, sizeof(info->hostname) - 1);
            info->hostname[sizeof(info->hostname) - 1] = '\0';
        } else if (strcmp(line, "DESCRIPTOR") == 0) {
            strncpy(info->descriptor, equals, sizeof(info->descriptor) - 1);
            info->descriptor[sizeof(info->descriptor) - 1] = '\0';
        } else if (strcmp(line, "COMMAND") == 0) {
            strncpy(info->cmdline, equals, sizeof(info->cmdline) - 1);
            info->cmdline[sizeof(info->cmdline) - 1] = '\0';
        }
    }
    
    fclose(fp);
    return 0;
}

/* Read lock file in any format (binary or text fallback) */
int read_lock_file_any_format(const char *path, struct lock_info *info) {
    int fd;
    
    /* Try binary format first */
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        if (read(fd, info, sizeof(*info)) == sizeof(*info)) {
            close(fd);
            if (info->magic == LOCK_MAGIC) {
                return 0;  /* Binary format successful */
            }
        }
        close(fd);
    }
    
    /* Binary format failed, try text format */
    if (read_text_lock_file(path, info) == 0) {
        debug("Read lock file using text fallback format");
        return 0;
    }
    
    return -1;  /* Both formats failed */
}

/* Signal a waiting process to release its lock */
int done_lock(const char *descriptor) {
    char *lock_dir;
    char lock_path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    struct lock_info info;
    size_t desc_len;
    int found_locks = 0;
    int released_locks = 0;
    
    /* Find lock directory */
    lock_dir = find_lock_directory();
    if (!lock_dir) {
        error(E_NODIR, "Cannot find or create lock directory");
        return E_NODIR;
    }
    
    /* Open lock directory */
    dir = opendir(lock_dir);
    if (!dir) {
        error(E_SYSTEM, "Cannot open lock directory %s: %s", lock_dir, strerror(errno));
        return E_SYSTEM;
    }
    
    desc_len = strlen(descriptor);
    
    /* Search for lock files matching the descriptor */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. entries */
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        /* Check if this file matches our descriptor pattern */
        if (strncmp(entry->d_name, descriptor, desc_len) == 0 &&
            (entry->d_name[desc_len] == '.' || entry->d_name[desc_len] == '\0') &&
            strstr(entry->d_name, ".lock")) {
            
            /* Construct full path */
            safe_snprintf(lock_path, sizeof(lock_path), "%s/%s", lock_dir, entry->d_name);
            
            /* Read lock file information */
            if (read_lock_file_any_format(lock_path, &info) == 0) {
                /* Validate checksum */
                if (validate_lock_checksum(&info)) {
                    found_locks++;
                    
                    /* Check if process is still alive */
                    if (process_exists(info.pid)) {
                        /* Send SIGTERM to the process */
                        if (kill(info.pid, SIGTERM) == 0) {
                            debug("Sent SIGTERM to process %d for lock %s", info.pid, descriptor);
                            released_locks++;
                            
                            /* Log to syslog if enabled */
                            if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                                openlog("waitlock", LOG_PID, g_state.syslog_facility);
                                syslog(LOG_INFO, "signaled process %d to release lock '%s'", info.pid, descriptor);
                                closelog();
#endif
                            }
                        } else {
                            debug("Failed to send SIGTERM to process %d: %s", info.pid, strerror(errno));
                            
                            /* If signal failed, check if process is actually gone */
                            if (!process_exists(info.pid)) {
                                debug("Process %d no longer exists, removing stale lock", info.pid);
                                unlink(lock_path);
                                released_locks++;
                            }
                        }
                    } else {
                        /* Process is dead, remove stale lock */
                        debug("Process %d no longer exists, removing stale lock", info.pid);
                        unlink(lock_path);
                        released_locks++;
                    }
                } else {
                    debug("Invalid checksum in lock file %s, removing", lock_path);
                    unlink(lock_path);
                }
            } else {
                debug("Failed to read lock file %s, removing", lock_path);
                unlink(lock_path);
            }
        }
    }
    
    closedir(dir);
    
    if (found_locks == 0) {
        if (!g_state.quiet) {
            error(E_NOTFOUND, "No locks found for descriptor '%s'", descriptor);
        }
        return E_NOTFOUND;
    }
    
    if (released_locks == 0) {
        if (!g_state.quiet) {
            error(E_SYSTEM, "Failed to release any locks for descriptor '%s'", descriptor);
        }
        return E_SYSTEM;
    }
    
    debug("Released %d lock(s) for descriptor '%s'", released_locks, descriptor);
    return E_SUCCESS;
}