/*
 * Process management functions - existence checking, command line extraction, exec mode
 */

#include "process.h"
#include "../core/core.h"
#include "../lock/lock.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include <signal.h>

/* Check if process exists */
bool process_exists(pid_t pid) {
    if (pid <= 0) return FALSE;
    
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    /* Use sysctl for BSD/macOS systems */
    int mib[4];
    size_t len;
    struct kinfo_proc kp;
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    
    len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        return TRUE;
    }
    return FALSE;
#else
    /* Use kill(0) for Linux and other POSIX systems */
    if (kill(pid, 0) == 0) {
        return TRUE;
    }
    
    if (errno == ESRCH) {
        return FALSE;
    }
    
    if (errno == EPERM) {
        return TRUE;
    }
    
    return FALSE;
#endif
}

/* Get process command line */
char* get_process_cmdline(pid_t pid) {
    static char cmdline[MAX_CMDLINE];
    
#ifdef __linux__
    char proc_path[64];
    int fd;
    ssize_t len;
    int i;
    
    safe_snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", (int)pid);
    fd = open(proc_path, O_RDONLY);
    if (fd < 0) return NULL;
    
    len = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    
    if (len <= 0) return NULL;
    cmdline[len] = '\0';
    
    /* Replace nulls with spaces */
    for (i = 0; i < len - 1; i++) {
        if (cmdline[i] == '\0') cmdline[i] = ' ';
    }
    
    return cmdline;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    /* Use sysctl for BSD/macOS systems */
    int mib[4];
    size_t len;
    struct kinfo_proc kp;
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    
    len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) != 0 || len == 0) {
        return NULL;
    }
    
#ifdef __APPLE__
    /* On macOS, use kp_proc.p_comm for basic command name */
    strncpy(cmdline, kp.kp_proc.p_comm, sizeof(cmdline) - 1);
    cmdline[sizeof(cmdline) - 1] = '\0';
#else
    /* On BSD, use ki_comm for basic command name */
    strncpy(cmdline, kp.ki_comm, sizeof(cmdline) - 1);
    cmdline[sizeof(cmdline) - 1] = '\0';
#endif
    
    return cmdline;
#else
    /* Fallback: try ps command */
    FILE *fp;
    char ps_cmd[128];
    
    safe_snprintf(ps_cmd, sizeof(ps_cmd), "ps -p %d -o args= 2>/dev/null", (int)pid);
    fp = popen(ps_cmd, "r");
    if (fp == NULL) return NULL;
    
    if (fgets(cmdline, sizeof(cmdline), fp) == NULL) {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    
    /* Remove trailing newline */
    size_t len = strlen(cmdline);
    if (len > 0 && cmdline[len-1] == '\n') {
        cmdline[len-1] = '\0';
    }
    
    return cmdline;
#endif
}

/* Execute command while holding lock */
int exec_with_lock(const char *descriptor, char *argv[]) {
    int ret;
    pid_t pid;
    int status;
    
    /* Acquire lock first */
    ret = acquire_lock(descriptor, opts.max_holders, opts.timeout);
    if (ret != E_SUCCESS) {
        return ret;
    }
    
    /* Fork and exec */
    pid = fork();
    if (pid < 0) {
        error(E_SYSTEM, "Cannot fork to execute command: %s", strerror(errno));
        release_lock();
        return E_SYSTEM;
    }
    
    if (pid == 0) {
        /* Child process - reset signal handlers to default */
        signal(SIGTERM, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        
        /* Export WAITLOCK_SLOT environment variable for semaphore holders */
        if (opts.max_holders > 1) {
            char slot_str[32];
            /* Need to get the slot number from the lock file path */
            char *slot_pos = strstr(g_state.lock_path, ".slot");
            if (slot_pos) {
                int slot_num = atoi(slot_pos + 5);  /* Skip ".slot" */
                safe_snprintf(slot_str, sizeof(slot_str), "%d", slot_num);
                setenv("WAITLOCK_SLOT", slot_str, 1);
            }
        }
        
        execvp(argv[0], argv);
        error(E_EXEC, "Cannot execute command '%s': %s", argv[0], strerror(errno));
        exit((errno == ENOENT) ? E_NOTFOUND : E_EXEC);
    }
    
    /* Parent process - set child PID for signal forwarding */
    g_state.child_pid = pid;
    
    /* Log exec start to syslog */
    if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
        openlog("waitlock", LOG_PID, g_state.syslog_facility);
        syslog(LOG_INFO, "started exec process %d: %s", (int)pid, argv[0]);
        closelog();
#endif
    }
    
    /* Wait for child */
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            error(E_SYSTEM, "waitpid failed for child process %d: %s", pid, strerror(errno));
            g_state.child_pid = 0;  /* Clear child PID */
            release_lock();
            return E_SYSTEM;
        }
        
        /* If we received a signal and should exit, try to terminate child cleanly */
        if (g_state.should_exit) {
            /* Send SIGTERM to child if it's still running */
            if (kill(pid, 0) == 0) {
                kill(pid, SIGTERM);
                /* Give child a moment to terminate cleanly */
                sleep(1);
                /* If still running, send SIGKILL */
                if (kill(pid, 0) == 0) {
                    kill(pid, SIGKILL);
                }
            }
        }
    }
    
    /* Clear child PID when done */
    g_state.child_pid = 0;
    
    release_lock();
    
    /* Log exec completion to syslog */
    if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
        openlog("waitlock", LOG_PID, g_state.syslog_facility);
        if (WIFEXITED(status)) {
            syslog(LOG_INFO, "exec process %d exited with status %d", 
                   (int)pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            syslog(LOG_INFO, "exec process %d terminated by signal %d", 
                   (int)pid, WTERMSIG(status));
        } else {
            syslog(LOG_WARNING, "exec process %d terminated abnormally", (int)pid);
        }
        closelog();
#endif
    }
    
    /* Return child's exit status */
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    } else {
        return E_SYSTEM;
    }
}