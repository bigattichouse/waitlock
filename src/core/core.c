/*
 * Core functionality - command line parsing, utilities, error handling
 */

#include "core.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/sysctl.h>
#endif

/* C89 compatibility function for strcasecmp */
int strcasecmp_compat(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* Parse syslog facility name to facility constant */
int parse_syslog_facility(const char *facility_name) {
    if (facility_name == NULL) {
        return -1;
    }
#ifdef HAVE_SYSLOG_H
#ifdef HAVE_STRCASECMP
    #define STRCASECMP strcasecmp
#else
    #define STRCASECMP strcasecmp_compat
#endif
    
    if (STRCASECMP(facility_name, "daemon") == 0) return LOG_DAEMON;
    if (STRCASECMP(facility_name, "local0") == 0) return LOG_LOCAL0;
    if (STRCASECMP(facility_name, "local1") == 0) return LOG_LOCAL1;
    if (STRCASECMP(facility_name, "local2") == 0) return LOG_LOCAL2;
    if (STRCASECMP(facility_name, "local3") == 0) return LOG_LOCAL3;
    if (STRCASECMP(facility_name, "local4") == 0) return LOG_LOCAL4;
    if (STRCASECMP(facility_name, "local5") == 0) return LOG_LOCAL5;
    if (STRCASECMP(facility_name, "local6") == 0) return LOG_LOCAL6;
    if (STRCASECMP(facility_name, "local7") == 0) return LOG_LOCAL7;
    if (STRCASECMP(facility_name, "user") == 0) return LOG_USER;
    if (STRCASECMP(facility_name, "mail") == 0) return LOG_MAIL;
    if (STRCASECMP(facility_name, "news") == 0) return LOG_NEWS;
    if (STRCASECMP(facility_name, "uucp") == 0) return LOG_UUCP;
    if (STRCASECMP(facility_name, "cron") == 0) return LOG_CRON;
    if (STRCASECMP(facility_name, "authpriv") == 0) return LOG_AUTHPRIV;
    if (STRCASECMP(facility_name, "syslog") == 0) return LOG_SYSLOG;
#endif
    return -1;  /* Invalid facility */
}

/* Portable string functions */
#ifndef HAVE_SNPRINTF
static int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    int ret = vsprintf(str, format, args);
    if (ret >= (int)size) {
        str[size - 1] = '\0';
        ret = (int)size - 1;
    }
    return ret;
}

static int snprintf_impl(char *str, size_t size, const char *format, ...) {
    va_list args;
    int ret;
    
    va_start(args, format);
    ret = vsnprintf(str, size, format, args);
    va_end(args);
    
    return ret;
}

#define snprintf snprintf_impl
#endif

/* Parse command line arguments */
int parse_args(int argc, char *argv[]) {
    int i;
    char *env_timeout, *env_dir, *env_slot;
    
    /* Check environment variables first */
    env_timeout = getenv("WAITLOCK_TIMEOUT");
    if (env_timeout) {
        opts.timeout = atof(env_timeout);
        if (opts.timeout < 0.0) {
            error(E_USAGE, "WAITLOCK_TIMEOUT must be non-negative");
            return E_USAGE;
        }
    }
    
    env_dir = getenv("WAITLOCK_DIR");
    if (env_dir) {
        opts.lock_dir = env_dir;
    }
    
    env_slot = getenv("WAITLOCK_SLOT");
    if (env_slot) {
        opts.preferred_slot = atoi(env_slot);
        if (opts.preferred_slot < 0) {
            opts.preferred_slot = -1;  /* Invalid slot, use auto */
        }
    }
    
    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            exit(E_SUCCESS);
        }
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            version();
            exit(E_SUCCESS);
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--allowMultiple") == 0) {
            if (++i >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i-1]);
                return E_USAGE;
            }
            opts.max_holders = atoi(argv[i]);
            if (opts.max_holders < 1) {
                error(E_USAGE, "Invalid value for --allowMultiple: %s (must be a positive integer)", argv[i]);
                return E_USAGE;
            }
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--onePerCPU") == 0) {
            opts.one_per_cpu = TRUE;
        }
        else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--excludeCPUs") == 0) {
            if (++i >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i-1]);
                return E_USAGE;
            }
            opts.exclude_cpus = atoi(argv[i]);
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (++i >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i-1]);
                return E_USAGE;
            }
            opts.timeout = atof(argv[i]);
            if (opts.timeout < 0.0) {
                error(E_USAGE, "Timeout must be non-negative");
                return E_USAGE;
            }
        }
        else if (strcmp(argv[i], "--check") == 0) {
            opts.check_only = TRUE;
        }
        else if (strcmp(argv[i], "--done") == 0) {
            opts.done_mode = TRUE;
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            opts.list_mode = TRUE;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            opts.show_all = TRUE;
        }
        else if (strcmp(argv[i], "--stale-only") == 0) {
            opts.stale_only = TRUE;
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) {
            if (++i >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i-1]);
                return E_USAGE;
            }
            if (strcmp(argv[i], "human") == 0) {
                opts.output_format = FMT_HUMAN;
            } else if (strcmp(argv[i], "csv") == 0) {
                opts.output_format = FMT_CSV;
            } else if (strcmp(argv[i], "null") == 0) {
                opts.output_format = FMT_NULL;
            } else {
                error(E_USAGE, "Unknown format: %s (supported formats: human, csv, null)", argv[i]);
                return E_USAGE;
            }
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--lock-dir") == 0) {
            if (++i >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i-1]);
                return E_USAGE;
            }
            opts.lock_dir = argv[i];
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            g_state.quiet = TRUE;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_state.verbose = TRUE;
        }
        else if (strcmp(argv[i], "--syslog") == 0) {
            g_state.use_syslog = TRUE;
        }
        else if (strcmp(argv[i], "--syslog-facility") == 0) {
            if (i + 1 >= argc) {
                error(E_USAGE, "Option %s requires an argument", argv[i]);
                return E_USAGE;
            }
            i++;
            g_state.syslog_facility = parse_syslog_facility(argv[i]);
            if (g_state.syslog_facility == -1) {
                error(E_USAGE, "Invalid syslog facility: %s (supported: daemon, local0-local7)", argv[i]);
                return E_USAGE;
            }
        }
        else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--exec") == 0) {
            if (i + 1 >= argc) {
                error(E_USAGE, "Option %s requires arguments", argv[i]);
                return E_USAGE;
            }
            opts.exec_argv = &argv[i + 1];
            break;  /* Rest of args are for exec */
        }
        else if (strcmp(argv[i], "--test") == 0) {
            opts.test_mode = TRUE;
        }
        else if (argv[i][0] == '-') {
            error(E_USAGE, "Unknown option: %s", argv[i]);
            return E_USAGE;
        }
        else {
            /* First non-option argument is the descriptor */
            if (!opts.descriptor) {
                opts.descriptor = argv[i];
            } else {
                error(E_USAGE, "Unexpected argument: %s", argv[i]);
                return E_USAGE;
            }
        }
    }
    
    /* Handle CPU-based limits */
    if (opts.one_per_cpu) {
        int cpu_count = get_cpu_count();
        opts.max_holders = cpu_count - opts.exclude_cpus;
        if (opts.max_holders < 1) opts.max_holders = 1;
    }
    
    /* Read descriptor from stdin if not provided */
    if (!opts.list_mode && !opts.test_mode && !opts.descriptor) {
        static char stdin_desc[MAX_DESC_LEN + 1];
        if (fgets(stdin_desc, sizeof(stdin_desc), stdin)) {
            size_t len = strlen(stdin_desc);
            if (len > 0 && stdin_desc[len-1] == '\n') {
                stdin_desc[len-1] = '\0';
            }
            opts.descriptor = stdin_desc;
        }
    }
    
    /* Validate descriptor */
    if (!opts.list_mode && !opts.test_mode && opts.descriptor) {
        const char *p;
        for (p = opts.descriptor; *p; p++) {
            if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.') {
                error(E_USAGE, "Invalid descriptor: %s (only alphanumeric characters, underscores, hyphens, and dots allowed)", opts.descriptor);
                return E_USAGE;
            }
        }
        if (strlen(opts.descriptor) > MAX_DESC_LEN) {
            error(E_USAGE, "Descriptor too long: %zu characters (max %d)", strlen(opts.descriptor), MAX_DESC_LEN);
            return E_USAGE;
        }
    }
    
    /* Check required arguments */
    if (!opts.list_mode && !opts.test_mode && !opts.descriptor) {
        error(E_USAGE, "No descriptor specified (provide as argument or via stdin)");
        return E_USAGE;
    }
    
    /* Validate preferred slot */
    if (opts.preferred_slot >= 0 && opts.preferred_slot >= opts.max_holders) {
        error(E_USAGE, "Preferred slot %d is out of range (0-%d)", 
              opts.preferred_slot, opts.max_holders - 1);
        return E_USAGE;
    }
    
    return 0;
}

/* Usage message */
void usage(FILE *stream) {
    fprintf(stream, "Usage: waitlock [options] <descriptor>\n");
    fprintf(stream, "       waitlock --list [--format=<fmt>] [--all|--stale-only]\n");
    fprintf(stream, "       waitlock --check <descriptor>\n");
    fprintf(stream, "       waitlock --done <descriptor>\n");
    fprintf(stream, "       echo <descriptor> | waitlock [options]\n");
    fprintf(stream, "\n");
    fprintf(stream, "Process synchronization tool for shell scripts.\n");
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -m, --allowMultiple N    Allow N concurrent holders (semaphore)\n");
    fprintf(stream, "  -c, --onePerCPU          Allow one lock per CPU core\n");
    fprintf(stream, "  -x, --excludeCPUs N      Reserve N CPUs (with --onePerCPU)\n");
    fprintf(stream, "  -t, --timeout SECS       Timeout in seconds (default: infinite)\n");
    fprintf(stream, "  --check                  Test if lock is available\n");
    fprintf(stream, "  --done                   Signal lock holder to release lock\n");
    fprintf(stream, "  -e, --exec CMD           Execute command while holding lock\n");
    fprintf(stream, "  -l, --list               List active locks\n");
    fprintf(stream, "  -a, --all                Include stale locks in list\n");
    fprintf(stream, "  --stale-only             Show only stale locks\n");
    fprintf(stream, "  -f, --format FMT         Output format: human, csv, null\n");
    fprintf(stream, "  -d, --lock-dir DIR       Lock directory (default: auto)\n");
    fprintf(stream, "  -q, --quiet              Suppress non-error output\n");
    fprintf(stream, "  -v, --verbose            Verbose output\n");
    fprintf(stream, "  --syslog                 Log to syslog\n");
    fprintf(stream, "  --syslog-facility FAC    Syslog facility (daemon|local0-7)\n");
    fprintf(stream, "  --test                   Run internal test suite\n");
    fprintf(stream, "  -h, --help               Show this help\n");
    fprintf(stream, "  -V, --version            Show version\n");
}

/* Version message */
void version(void) {
    printf("waitlock %s\n", VERSION);
}

/* Debug output */
void debug(const char *fmt, ...) {
    va_list args;
    
    if (!g_state.verbose) return;
    
    fprintf(stderr, "waitlock[%d]: DEBUG: ", (int)getpid());
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* Error output */
void error(int code, const char *fmt, ...) {
    va_list args;
    
    if (g_state.quiet && code != E_USAGE) return;
    
    fprintf(stderr, "waitlock: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* Safe snprintf wrapper */
int safe_snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;
    
    va_start(args, fmt);
#ifdef HAVE_SNPRINTF
    ret = vsnprintf(buf, size, fmt, args);
#else
    ret = vsprintf(buf, fmt, args);
    if (ret >= (int)size) {
        buf[size - 1] = '\0';
        ret = (int)size - 1;
    }
#endif
    va_end(args);
    
    return ret;
}

/* Get CPU count using portable methods */
int get_cpu_count(void) {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    /* Use sysctl for BSD/macOS systems */
    int cpu_count;
    size_t len = sizeof(cpu_count);
    
    if (sysctlbyname("hw.ncpu", &cpu_count, &len, NULL, 0) == 0) {
        return (cpu_count > 0) ? cpu_count : 1;
    }
    
    /* Fallback to hw.logicalcpu on macOS */
#ifdef __APPLE__
    if (sysctlbyname("hw.logicalcpu", &cpu_count, &len, NULL, 0) == 0) {
        return (cpu_count > 0) ? cpu_count : 1;
    }
#endif
    
    return 1;  /* Fallback */
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
    /* Use sysconf for Linux and other POSIX systems */
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    return (cpu_count > 0) ? cpu_count : 1;
#else
    /* Final fallback */
    return 1;
#endif
}