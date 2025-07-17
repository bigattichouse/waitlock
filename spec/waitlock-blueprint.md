# WaitLock Tool - BluePrint Design Document

## Overview

WaitLock is a portable UNIX/POSIX command-line tool that provides mutex and semaphore functionality for shell scripts. It enables synchronized access to resources across multiple processes with automatic cleanup when processes die.

## Core Features

- **Mutex Mode**: Single lock holder (default)
- **Semaphore Mode**: Multiple concurrent lock holders
- **Automatic Cleanup**: Locks released when process dies
- **CPU-aware Locking**: Can scale locks to CPU count
- **Lock Inspection**: List and check active locks
- **Multiple Output Formats**: Human, CSV, and null-separated
- **Command Execution**: Run commands while holding locks
- **UNIX Integration**: Environment variables, stdin, syslog
- **Portable C Implementation**: Runs on any POSIX system

## Command Interface

```blueprint
Tool WaitLock {
  name: "waitlock",
  version: "1.0.0",
  
  Commands {
    primary_usage: "waitlock [options] <descriptor>",
    stdin_usage: "echo <descriptor> | waitlock [options]",
    management_usage: "waitlock --list [options]",
    
    CoreOptions [
      {
        flag: "--allowMultiple, -m",
        argument: "count",
        type: "integer",
        description: "Allow up to N concurrent lock holders (semaphore mode)"
      },
      {
        flag: "--onePerCPU, -c",
        description: "Allow one lock per CPU core"
      },
      {
        flag: "--excludeCPUs, -x",
        argument: "count",
        type: "integer",
        description: "Reserve N CPUs (reduce available locks by N)"
      },
      {
        flag: "--timeout, -t",
        argument: "seconds",
        type: "float",
        default: "${WAITLOCK_TIMEOUT:-infinite}",
        description: "Maximum wait time before giving up"
      },
      {
        flag: "--check",
        description: "Test if lock is available without acquiring (exit 0 if available)"
      },
      {
        flag: "--exec, -e",
        argument: "command",
        description: "Execute command while holding lock"
      }
    ],
    
    OutputOptions [
      {
        flag: "--quiet, -q",
        description: "Suppress all non-error output"
      },
      {
        flag: "--verbose, -v",
        description: "Verbose output for debugging"
      },
      {
        flag: "--format, -f",
        argument: "format",
        choices: ["human", "csv", "null"],
        default: "human",
        description: "Output format for --list"
      },
      {
        flag: "--syslog",
        description: "Log operations to syslog"
      },
      {
        flag: "--syslog-facility",
        argument: "facility",
        default: "daemon",
        description: "Syslog facility (daemon|local0-7)"
      }
    ],
    
    ManagementOptions [
      {
        flag: "--list, -l",
        description: "List active locks and exit"
      },
      {
        flag: "--all, -a",
        description: "With --list, include potentially stale locks"
      },
      {
        flag: "--stale-only",
        description: "With --list, show only stale locks"
      }
    ],
    
    ConfigurationOptions [
      {
        flag: "--lock-dir, -d",
        argument: "path",
        type: "directory",
        default: "${WAITLOCK_DIR:-auto}",
        description: "Directory for lock files"
      },
      {
        flag: "--help, -h",
        description: "Show usage information"
      },
      {
        flag: "--version, -V",
        description: "Show version information"
      }
    ]
  }
}
```

## Environment Variables

```blueprint
EnvironmentVariables {
  WAITLOCK_DIR: {
    description: "Default lock directory path",
    default: "auto-detect",
    example: "WAITLOCK_DIR=/var/run/locks waitlock myapp"
  },
  
  WAITLOCK_TIMEOUT: {
    description: "Default timeout in seconds",
    default: "infinite",
    example: "WAITLOCK_TIMEOUT=30 waitlock db_backup"
  },
  
  WAITLOCK_DEBUG: {
    description: "Enable debug output to stderr",
    values: "1, true, yes (case insensitive)",
    example: "WAITLOCK_DEBUG=1 waitlock --list"
  }
}
```

## Exit Codes

```blueprint
ExitCodes {
  StandardMode {
    0: "Success - lock acquired and released cleanly",
    1: "Lock held by another process (mutex conflict or semaphore full)",
    2: "Timeout expired while waiting for lock",
    3: "Invalid command line arguments",
    4: "System error (check errno/stderr)",
    5: "Permission denied on lock file or directory",
    6: "Lock directory not accessible or cannot be created",
    75: "Temporary failure (EX_TEMPFAIL) - retry might succeed"
  },
  
  CheckMode {
    0: "Lock is available (could be acquired)",
    1: "Lock is currently held",
    2: "Error checking lock status"
  },
  
  ListMode {
    0: "Command executed successfully (locks may or may not exist)",
    1: "Error during execution"
  },
  
  ExecMode {
    "Exits with status of executed command, or 126/127 for exec failures"
  }
}
```

## Architecture

```blueprint
Architecture WaitLock {
  
  CoreComponents {
    
    LockManager {
      description: "Central lock acquisition and management logic",
      
      properties: {
        descriptor: "char[256]",
        lock_directory: "char[PATH_MAX]",
        max_holders: "int",
        timeout_seconds: "double",
        check_only: "bool",
        exec_command: "char*"
      },
      
      methods: [
        initialize(options) -> "error_code",
        acquire_lock() -> "bool",
        wait_for_lock() -> "bool",
        release_lock() -> "void",
        cleanup_stale_locks() -> "int count_cleaned"
      ]
    },
    
    LockFile {
      description: "Individual lock file representation",
      
      format: {
        filename: "<descriptor>.<hostname>.<pid>.lock",
        example: "database_backup.server01.12345.lock"
      },
      
      content_structure: {
        version: "int",
        pid: "pid_t",
        ppid: "pid_t",
        uid: "uid_t",
        hostname: "char[256]",
        command_line: "char[4096]",
        descriptor: "char[256]",
        acquired_at: "time_t",
        lock_type: "enum {MUTEX, SEMAPHORE}",
        max_holders: "int"
      }
    },
    
    LockDirectory {
      description: "Lock directory management and discovery",
      
      search_order: [
        "/var/run/waitlock",
        "/run/waitlock",
        "/var/lock/waitlock",
        "/tmp/waitlock",
        "${HOME}/.waitlock",
        "./waitlock"
      ],
      
      methods: [
        find_or_create_directory() -> "const char*",
        verify_permissions() -> "bool",
        list_lock_files(descriptor_filter) -> "file_list"
      ]
    },
    
    ProcessManager {
      description: "Process detection and command line extraction",
      
      methods: [
        process_exists(pid) -> "bool",
        get_process_cmdline(pid) -> "char*",
        is_same_process(lock_info, current_pid) -> "bool",
        send_signal_to_process(pid, signal) -> "error_code"
      ]
    },
    
    LockLister {
      description: "Lock listing and reporting functionality",
      
      output_formats: {
        human: {
          header: "DESCRIPTOR          PID    USER     ACQUIRED             COMMAND",
          row:    "%-18s %-6d %-8s %-19s %s",
          
          example: [
            "DESCRIPTOR          PID    USER     ACQUIRED             COMMAND",
            "database_backup     12345  root     2024-01-15 10:23:45  /usr/local/bin/backup.sh --daily",
            "config_update       23456  admin    2024-01-15 10:24:12  ./update_config.py --force",
            "  [STALE]          (1234)  root     2024-01-15 09:15:00  Process no longer exists"
          ]
        },
        
        csv: {
          header: "descriptor,pid,user,acquired,status,command",
          row: "%s,%d,%s,%ld,%s,%s",
          example: "database_backup,12345,root,1705332225,active,/usr/local/bin/backup.sh --daily"
        },
        
        null: {
          /* Null-separated output for safe parsing */
          format: "descriptor\0pid\0user\0acquired\0status\0command\0",
          notes: "All fields null-terminated, records double-null-terminated"
        }
      },
      
      methods: [
        list_all_locks(format, show_stale, stale_only) -> "void",
        format_lock_entry(lock_info, format) -> "void",
        check_stale_status(lock_info) -> "enum {ACTIVE, STALE, UNKNOWN}"
      ]
    },
    
    CheckMode {
      description: "Test lock availability without acquiring",
      
      methods: [
        check_lock_available(descriptor) -> "bool",
        count_active_locks(descriptor) -> "int",
        get_lock_holders(descriptor) -> "pid_list"
      ]
    },
    
    ExecMode {
      description: "Execute command while holding lock",
      
      behavior: [
        "Acquire lock before fork/exec",
        "Pass signals to child process",
        "Release lock after child exits",
        "Exit with child's exit status"
      ],
      
      signal_handling: "Forward all catchable signals to child"
    },
    
    StdinReader {
      description: "Read descriptor from stdin if not provided",
      
      methods: [
        read_descriptor_stdin() -> "char*",
        validate_descriptor(input) -> "bool"
      ]
    },
    
    SyslogIntegration {
      description: "Optional syslog logging for audit trails",
      
      log_events: [
        "Lock acquired: waitlock[PID]: acquired lock 'descriptor' for 'command'",
        "Lock released: waitlock[PID]: released lock 'descriptor' after N seconds",
        "Lock timeout: waitlock[PID]: timeout waiting for lock 'descriptor'",
        "Lock conflict: waitlock[PID]: lock 'descriptor' held by PID owner"
      ],
      
      implementation: {
        facility_map: {
          "daemon": "LOG_DAEMON",
          "local0-7": "LOG_LOCAL0 through LOG_LOCAL7"
        },
        
        priority: "LOG_INFO for normal operations, LOG_WARNING for conflicts"
      }
    }
  }
}
```

## Portable C Implementation

```blueprint
PortableImplementation {
  
  CStandard {
    version: "C89/C90 (ANSI C)",
    posix: "POSIX.1-2001",
    
    compiler_flags: {
      strict: "-ansi -pedantic -Wall -Wextra",
      optimization: "-O2",
      debug: "-g -DDEBUG"
    }
  },
  
  FeatureDetection {
    configure_checks: [
      "AC_CHECK_FUNCS([flock fcntl lockf])",
      "AC_CHECK_FUNCS([snprintf vsnprintf])",
      "AC_CHECK_FUNCS([sysconf])",
      "AC_CHECK_FUNCS([readlink])",
      "AC_CHECK_HEADERS([sys/file.h sys/param.h])",
      "AC_CHECK_DECLS([_SC_NPROCESSORS_ONLN])"
    ]
  },
  
  PortablePatterns {
    
    FileLocking {
      /* Portable locking with fallbacks */
      int portable_lock(int fd, int operation) {
        #ifdef HAVE_FLOCK
          return flock(fd, operation);
        #elif defined(HAVE_FCNTL)
          struct flock fl;
          memset(&fl, 0, sizeof(fl));
          fl.l_type = (operation & LOCK_EX) ? F_WRLCK : F_RDLCK;
          fl.l_whence = SEEK_SET;
          fl.l_start = 0;
          fl.l_len = 0;
          int cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;
          return fcntl(fd, cmd, &fl);
        #else
          int op = (operation & LOCK_EX) ? F_LOCK : F_TEST;
          if (operation & LOCK_NB) op = F_TLOCK;
          return lockf(fd, op, 0);
        #endif
      }
    },
    
    ProcessCmdline {
      /* Portable command line extraction */
      char* get_process_cmdline(pid_t pid) {
        static char cmdline[4096];
        
        #ifdef __linux__
          char proc_path[64];
          int fd;
          ssize_t len;
          
          snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", (int)pid);
          fd = open(proc_path, O_RDONLY);
          if (fd < 0) return NULL;
          
          len = read(fd, cmdline, sizeof(cmdline) - 1);
          close(fd);
          
          if (len <= 0) return NULL;
          cmdline[len] = '\0';
          
          /* Replace nulls with spaces */
          for (int i = 0; i < len - 1; i++) {
            if (cmdline[i] == '\0') cmdline[i] = ' ';
          }
          
        #elif defined(__FreeBSD__) || defined(__APPLE__)
          /* Use sysctl on BSD systems */
          int mib[4];
          size_t len = sizeof(cmdline);
          
          mib[0] = CTL_KERN;
          mib[1] = KERN_PROC;
          mib[2] = KERN_PROC_ARGS;
          mib[3] = pid;
          
          if (sysctl(mib, 4, cmdline, &len, NULL, 0) < 0) {
            return NULL;
          }
          
        #else
          /* Fallback: try ps command */
          FILE *fp;
          char ps_cmd[128];
          
          snprintf(ps_cmd, sizeof(ps_cmd), "ps -p %d -o args= 2>/dev/null", (int)pid);
          fp = popen(ps_cmd, "r");
          if (fp == NULL) return NULL;
          
          if (fgets(cmdline, sizeof(cmdline), fp) == NULL) {
            pclose(fp);
            return NULL;
          }
          pclose(fp);
          
          /* Remove trailing newline */
          len = strlen(cmdline);
          if (len > 0 && cmdline[len-1] == '\n') {
            cmdline[len-1] = '\0';
          }
        #endif
        
        return cmdline;
      }
    },
    
    CPUCount {
      /* Portable CPU detection */
      int get_cpu_count(void) {
        #ifdef _SC_NPROCESSORS_ONLN
          long count = sysconf(_SC_NPROCESSORS_ONLN);
          if (count > 0) return (int)count;
        #endif
        
        #ifdef HW_NCPU
          int mib[2] = { CTL_HW, HW_NCPU };
          int count;
          size_t len = sizeof(count);
          if (sysctl(mib, 2, &count, &len, NULL, 0) == 0) {
            return count;
          }
        #endif
        
        #ifdef __linux__
          /* Parse /proc/cpuinfo as fallback */
          FILE *fp = fopen("/proc/cpuinfo", "r");
          if (fp) {
            char line[256];
            int count = 0;
            while (fgets(line, sizeof(line), fp)) {
              if (strncmp(line, "processor", 9) == 0) {
                count++;
              }
            }
            fclose(fp);
            if (count > 0) return count;
          }
        #endif
        
        return 1; /* Safe default */
      }
    }
  }
}
```

## Lock File Format

```blueprint
LockFileFormat {
  version: 1,
  
  BinaryFormat {
    /* Fixed-size binary format for atomic writes */
    struct lock_info {
      uint32_t magic;        /* 0x57414C4B "WALK" */
      uint32_t version;      /* Format version */
      pid_t pid;            /* Process ID */
      pid_t ppid;           /* Parent process ID */
      uid_t uid;            /* User ID */
      time_t acquired_at;   /* Acquisition timestamp */
      uint16_t lock_type;   /* 0=mutex, 1=semaphore */
      uint16_t max_holders; /* Max concurrent holders */
      char hostname[256];   /* Null-terminated hostname */
      char descriptor[256]; /* Lock descriptor */
      char cmdline[4096];   /* Process command line */
      uint32_t checksum;    /* Simple checksum */
    };
  },
  
  TextFallback {
    /* Human-readable format if binary fails */
    format: [
      "VERSION=1",
      "PID=%d",
      "PPID=%d", 
      "UID=%d",
      "ACQUIRED=%ld",
      "TYPE=%s",
      "MAX_HOLDERS=%d",
      "HOSTNAME=%s",
      "DESCRIPTOR=%s",
      "COMMAND=%s"
    ]
  }
}
```

## Error Handling

```blueprint
ErrorHandling {
  
  ErrorCodes {
    /* Exit codes follow UNIX conventions */
    SUCCESS: 0,              /* Operation completed successfully */
    E_BUSY: 1,              /* Lock is busy (held by another) */
    E_TIMEOUT: 2,           /* Lock acquisition timeout */
    E_USAGE: 3,             /* Invalid command line arguments */
    E_SYSTEM: 4,            /* System error (check errno) */
    E_NOPERM: 5,            /* Permission denied */
    E_NODIR: 6,             /* Lock directory issues */
    E_TEMPFAIL: 75,         /* Temporary failure (retry may succeed) */
    E_EXEC: 126,            /* Command found but not executable */
    E_NOTFOUND: 127         /* Command not found (--exec mode) */
  },
  
  ErrorReporting {
    /* All errors to stderr, respect --quiet flag */
    void report_error(int code, const char *context) {
      if (g_quiet && code != E_USAGE) return;
      
      const char *msg;
      switch(code) {
        case E_BUSY:     msg = "Lock is held by another process"; break;
        case E_TIMEOUT:  msg = "Lock acquisition timed out"; break;
        case E_USAGE:    msg = "Invalid arguments (try --help)"; break;
        case E_SYSTEM:   msg = strerror(errno); break;
        case E_NOPERM:   msg = "Permission denied"; break;
        case E_NODIR:    msg = "Lock directory not accessible"; break;
        case E_TEMPFAIL: msg = "Temporary failure"; break;
        default:         msg = "Unknown error"; break;
      }
      
      if (context && g_verbose) {
        fprintf(stderr, "waitlock: %s: %s\n", context, msg);
      } else {
        fprintf(stderr, "waitlock: %s\n", msg);
      }
    }
  },
  
  DebugOutput {
    /* When WAITLOCK_DEBUG=1 or --verbose */
    categories: [
      "Lock directory selection",
      "Lock file operations", 
      "Process detection",
      "Signal handling",
      "Stale lock cleanup"
    ],
    
    format: "waitlock[%d]: DEBUG: %s\n"
  }
}
```

## Signal Handling

```blueprint
SignalHandling {
  
  SignalsToHandle [
    SIGTERM,  /* Termination request */
    SIGINT,   /* Interrupt (Ctrl+C) */
    SIGHUP,   /* Hangup */
    SIGQUIT,  /* Quit */
    SIGPIPE   /* Broken pipe (ignore) */
  ],
  
  Implementation {
    /* Global for signal handler access */
    static struct lock_state {
      int lock_fd;
      char lock_path[PATH_MAX];
      volatile sig_atomic_t should_exit;
    } g_state = { -1, "", 0 };
    
    void signal_handler(int sig) {
      g_state.should_exit = 1;
      
      /* Attempt cleanup in handler (not ideal but necessary) */
      if (g_state.lock_fd >= 0) {
        close(g_state.lock_fd);
        unlink(g_state.lock_path);
      }
      
      /* Re-raise signal for proper exit code */
      signal(sig, SIG_DFL);
      raise(sig);
    }
    
    void install_signal_handlers(void) {
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = signal_handler;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = 0;
      
      sigaction(SIGTERM, &sa, NULL);
      sigaction(SIGINT, &sa, NULL);
      sigaction(SIGHUP, &sa, NULL);
      sigaction(SIGQUIT, &sa, NULL);
      
      /* Ignore SIGPIPE */
      signal(SIGPIPE, SIG_IGN);
    }
  }
}
```

## Usage Examples

```blueprint
UsageExamples {
  
  BasicMutex {
    description: "Simple mutual exclusion",
    
    script: [
      "#!/bin/sh",
      "# Wait for exclusive access to database",
      "waitlock database_backup || exit 1",
      "",
      "# Critical section",
      "perform_backup",
      "",
      "# Lock automatically released on exit"
    ]
  },
  
  CheckAndWait {
    description: "Check before waiting pattern",
    
    script: [
      "#!/bin/sh",
      "# Check if available first",
      "if waitlock --check database_backup; then",
      "    echo 'Lock available, acquiring...'",
      "else",
      "    echo 'Lock is busy, waiting...'",
      "fi",
      "waitlock database_backup"
    ]
  },
  
  ExecPattern {
    description: "Execute command with lock",
    
    script: [
      "# More UNIX-like: lock wraps command execution",
      "waitlock database_backup --exec '/usr/local/bin/backup.sh --daily'",
      "",
      "# Signals are properly forwarded to the child process"
    ]
  },
  
  PipelineUsage {
    description: "Using stdin for descriptor",
    
    script: [
      "# Generate dynamic lock names",
      "echo \"backup_$(date +%Y%m%d)\" | waitlock",
      "",
      "# Batch processing with unique locks",
      "find /data -name '*.csv' | while read file; do",
      "    basename \"$file\" | waitlock --exec \"process_csv '$file'\"",
      "done"
    ]
  },
  
  ParallelExecution {
    description: "Controlled parallelism with xargs",
    
    script: [
      "# Process files with max 4 parallel jobs",
      "find . -name '*.dat' | \\",
      "  xargs -P 8 -I {} sh -c \\",
      "  'waitlock -m 4 batch_processor --exec \"process_file {}\"'"
    ]
  },
  
  MachineReadableOutput {
    description: "Parse lock information",
    
    script: [
      "# Count active locks",
      "waitlock --list --format=csv | tail -n +2 | wc -l",
      "",
      "# Find locks older than 1 hour",
      "waitlock --list --format=csv | awk -F, \\",
      "  'NR>1 && systime()-$4 > 3600 {print $1}'",
      "",
      "# Safe parsing with null separation",
      "waitlock --list --format=null | \\",
      "  xargs -0 -n6 printf 'Lock: %s PID: %s\\n'"
    ]
  },
  
  Monitoring {
    description: "Lock monitoring patterns",
    
    script: [
      "# Watch locks in real-time",
      "watch -n 1 'waitlock --list'",
      "",
      "# Alert on long-held locks",
      "waitlock --list --format=csv | \\",
      "  awk -F, 'NR>1 && systime()-$4 > 300 {",
      "    print \"WARNING: Lock \" $1 \" held for over 5 minutes by PID \" $2",
      "  }'",
      "",
      "# Log all lock operations",
      "WAITLOCK_DEBUG=1 waitlock --syslog db_task"
    ]
  },
  
  ErrorHandling {
    description: "Robust error handling",
    
    script: [
      "#!/bin/sh",
      "# Handle different exit codes",
      "waitlock --timeout 30 critical_resource",
      "case $? in",
      "    0) echo 'Lock acquired' ;;",
      "    1) echo 'Lock busy' >&2; exit 1 ;;",
      "    2) echo 'Timeout expired' >&2; exit 1 ;;",
      "    *) echo 'Unexpected error' >&2; exit 1 ;;",
      "esac"
    ]
  },
  
  CleanupPatterns {
    description: "Stale lock cleanup",
    
    script: [
      "# List only stale locks",
      "waitlock --list --stale-only",
      "",
      "# Cron job for cleanup (run as root)",
      "# 0 * * * * waitlock --list --stale-only --format=null | \\",
      "#   xargs -0 -I{} rm -f /var/run/waitlock/{}.*.lock"
    ]
  }
}
```

## Composability Patterns

```blueprint
ComposabilityPatterns {
  
  ConditionalExecution {
    description: "Combine with shell logic",
    
    examples: [
      "# Only run if no other instance is running",
      "waitlock --check myapp && waitlock myapp --exec ./myapp",
      "",
      "# Fallback behavior",
      "waitlock --timeout 5 render || waitlock fallback_render"
    ]
  },
  
  ResourcePools {
    description: "Manage resource pools",
    
    example: [
      "# GPU allocation (4 GPUs available)",
      "gpu_id=$(waitlock -m 4 gpu_pool --exec 'echo $WAITLOCK_SLOT')",
      "export CUDA_VISIBLE_DEVICES=$gpu_id"
    ]
  },
  
  DistributedCoordination {
    description: "Coordinate across machines",
    
    example: [
      "# NFS-based distributed locking",
      "WAITLOCK_DIR=/mnt/shared/locks waitlock cluster_task",
      "",
      "# SSH-based coordination",
      "ssh server1 'waitlock remote_job' && run_distributed_task"
    ]
  }
}
```

## Build System

```blueprint
BuildSystem {
  
  ConfigureScript {
    description: "Autoconf-based configuration",
    
    configure_ac: [
      "AC_INIT([waitlock], [1.0.0])",
      "AC_CONFIG_HEADERS([config.h])",
      "",
      "# Compiler checks",
      "AC_PROG_CC",
      "AC_PROG_CC_C89",
      "",
      "# Header checks", 
      "AC_CHECK_HEADERS([sys/file.h sys/param.h])",
      "",
      "# Function checks",
      "AC_CHECK_FUNCS([flock fcntl lockf])",
      "AC_CHECK_FUNCS([snprintf vsnprintf])",
      "AC_CHECK_FUNCS([sysconf])",
      "",
      "# System capabilities",
      "AC_CHECK_DECLS([_SC_NPROCESSORS_ONLN], [], [], [[#include <unistd.h>]])",
      "",
      "AC_CONFIG_FILES([Makefile])",
      "AC_OUTPUT"
    ]
  },
  
  Makefile {
    description: "Portable Makefile.in template",
    
    content: [
      "CC = @CC@",
      "CFLAGS = @CFLAGS@ -I.",
      "LDFLAGS = @LDFLAGS@",
      "LIBS = @LIBS@",
      "",
      "PREFIX = @prefix@",
      "BINDIR = $(PREFIX)/bin",
      "MANDIR = $(PREFIX)/share/man/man1",
      "",
      "PROG = waitlock",
      "OBJS = waitlock.o lock_manager.o process_utils.o",
      "",
      "all: $(PROG)",
      "",
      "$(PROG): $(OBJS)",
      "\t$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)",
      "",
      "install: $(PROG)",
      "\tinstall -D -m 755 $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)",
      "\tinstall -D -m 644 $(PROG).1 $(DESTDIR)$(MANDIR)/$(PROG).1",
      "",
      "clean:",
      "\trm -f $(PROG) $(OBJS)",
      "",
      ".PHONY: all install clean"
    ]
  }
}
```

## Testing Strategy

```blueprint
TestingStrategy {
  
  UnitTests {
    description: "Portable test suite",
    
    test_categories: [
      {
        name: "Lock acquisition",
        tests: [
          "Single process mutex",
          "Multiple process mutex conflict", 
          "Semaphore counting",
          "CPU-based limits"
        ]
      },
      {
        name: "Process death handling",
        tests: [
          "Clean exit releases lock",
          "SIGKILL cleanup by next waiter",
          "Parent death detection",
          "Stale lock removal"
        ]
      },
      {
        name: "Portability",
        tests: [
          "flock() systems",
          "fcntl() systems",
          "lockf() fallback",
          "CPU detection methods"
        ]
      }
    ]
  },
  
  IntegrationTests {
    description: "Real-world scenarios",
    
    scenarios: [
      "Database backup coordination",
      "Build system parallelization",
      "Resource pool management",
      "Distributed locking over NFS"
    ]
  },
  
  PlatformTesting {
    description: "Platforms to verify",
    
    required: [
      "Linux (glibc)",
      "Linux (musl)",
      "FreeBSD",
      "OpenBSD",
      "macOS"
    ],
    
    optional: [
      "NetBSD",
      "Solaris/illumos",
      "AIX",
      "HP-UX",
      "Cygwin",
      "WSL"
    ]
  }
}
```

## Performance Considerations

```blueprint
Performance {
  
  Optimizations {
    lock_directory_caching: "Cache discovered lock directory path",
    
    exponential_backoff: {
      initial_ms: 10,
      max_ms: 1000,
      multiplier: 2.0,
      jitter: "Add 0-10% random jitter to prevent thundering herd"
    },
    
    batch_stale_cleanup: "Clean all stale locks in one directory scan",
    
    minimal_syscalls: "Reduce system calls in hot paths",
    
    lock_coalescing: "Check all locks for a descriptor in one pass"
  },
  
  Scalability {
    expected_limits: {
      concurrent_waiters: "10,000+ processes",
      lock_descriptors: "Limited by filesystem inodes",
      performance: "O(n) where n = active locks for descriptor"
    },
    
    recommendations: [
      "Use hierarchical descriptors for namespace separation",
      "Consider dedicated lock directory on tmpfs for performance",
      "Monitor lock directory size in production"
    ]
  }
}
```

## Security Considerations

```blueprint
Security {
  
  FilePermissions {
    lock_directory: "0755 or 01777 (sticky bit)",
    lock_files: "0644 (readable by all, writable by owner)",
    
    validation: [
      "Check directory ownership",
      "Verify no symlink attacks",
      "Validate lock file ownership before removal"
    ]
  },
  
  InputValidation {
    descriptor: {
      allowed: "Alphanumeric, underscore, dash, dot",
      regex: "^[A-Za-z0-9._-]+$",
      max_length: 255,
      examples: ["my_app", "backup.daily", "node-1", "v2.0"]
    },
    
    paths: {
      validation: "Canonicalize and verify no directory traversal",
      checks: ["No ..", "No symlinks in lock directory", "Absolute path resolution"]
    },
    
    stdin_input: {
      handling: "Read first line only, strip whitespace",
      max_read: 256
    }
  },
  
  PrivilegeSeparation {
    description: "Drop privileges when possible",
    
    operations_requiring_root: [
      "Force killing other users' processes",
      "Removing other users' lock files",
      "Creating locks in system directories"
    ]
  }
}
```

## Future Considerations

```blueprint
FutureConsiderations {
  
  PotentialEnhancements [
    {
      feature: "Lock slots",
      description: "Export WAITLOCK_SLOT for semaphore holders",
      rationale: "Useful for resource pool management (e.g., GPU selection)"
    },
    {
      feature: "Read/write locks", 
      description: "Multiple readers, single writer pattern",
      implementation: "--read and --write flags"
    },
    {
      feature: "Advisory timeout",
      description: "Warn but don't fail on expected lock duration",
      implementation: "--expect-duration for monitoring"
    }
  ],
  
  ExplicitNonGoals [
    "Network/distributed locking (use dedicated tools)",
    "Persistent lock state across reboots",
    "Lock priority or queuing",
    "Built-in lock migration",
    "Complex management interface"
  ]
}
```

## Implementation Timeline

```blueprint
ImplementationPhases {
  
  Phase1_Core {
    duration: "1 day",
    deliverables: [
      "Basic mutex functionality",
      "Lock file creation/removal",
      "Process death detection",
      "Signal handling"
    ]
  },
  
  Phase2_Features {
    duration: "1 day",
    deliverables: [
      "Semaphore support (--allowMultiple)",
      "CPU detection (--onePerCPU)",
      "Check mode (--check)",
      "Stdin descriptor reading"
    ]
  },
  
  Phase3_OutputExec {
    duration: "1 day",
    deliverables: [
      "Multiple output formats (csv, null)",
      "Lock listing with status",
      "Exec mode with signal forwarding",
      "Syslog integration"
    ]
  },
  
  Phase4_Portability {
    duration: "2 days",
    deliverables: [
      "Configure script",
      "Platform-specific implementations",
      "Fallback mechanisms",
      "Comprehensive testing"
    ]
  },
  
  Phase5_Documentation {
    duration: "1 day",
    deliverables: [
      "Man page",
      "README with examples",
      "Integration test suite",
      "Distribution packaging"
    ]
  }
}
```

## UNIX Philosophy Compliance

```blueprint
UNIXPhilosophy {
  
  Principles {
    DoOneThingWell: {
      description: "Focus on lock acquisition and release",
      implementation: [
        "Core function is managing lock lifecycle",
        "Avoid feature creep (no built-in cleanup daemon)",
        "Let other tools handle process management"
      ]
    },
    
    TextInterface: {
      description: "Text-based input/output",
      implementation: [
        "Read descriptor from command line or stdin",
        "Multiple output formats (human, csv, null)",
        "Parse-friendly error messages on stderr"
      ]
    },
    
    Composability: {
      description: "Work well with other programs",
      implementation: [
        "Clear exit codes for scripting",
        "Machine-readable outputs",
        "--exec mode for wrapping commands",
        "Stdin support for dynamic descriptors"
      ]
    },
    
    Silence: {
      description: "No unnecessary output",
      implementation: [
        "--quiet mode suppresses all non-error output",
        "Default mode only outputs on error",
        "Verbose mode available when needed"
      ]
    },
    
    Portability: {
      description: "Run everywhere",
      implementation: [
        "ANSI C89/C90 compliance",
        "POSIX.1-2001 system calls",
        "Graceful fallbacks for missing features"
      ]
    }
  },
  
  DesignChoices {
    NoComplexManagement: {
      rationale: "Let users compose their own management tools",
      examples: [
        "No built-in --kill functionality",
        "No --force-unlock option", 
        "Users can: kill $(waitlock --list --format=csv | grep mylock | cut -d, -f2)"
      ]
    },
    
    EnvironmentVariables: {
      rationale: "Allow configuration without modifying scripts",
      usage: "WAITLOCK_TIMEOUT=30 ./batch_job.sh"
    },
    
    StandardExitCodes: {
      rationale: "Predictable behavior in scripts",
      usage: "waitlock mylock || handle_lock_failure"
    }
  }
}
```

## Tool Separation Considerations

```blueprint
ToolSeparation {
  
  CoreTool {
    name: "waitlock",
    purpose: "Acquire, hold, and release locks",
    
    included_features: [
      "Lock acquisition (mutex/semaphore)",
      "Lock checking (--check)",
      "Lock listing (--list)",
      "Command execution (--exec)"
    ],
    
    explicitly_excluded: [
      "Force killing processes",
      "Force removing lock files",
      "Automatic cleanup daemon",
      "Lock migration or upgrade"
    ]
  },
  
  ComplementaryTools {
    description: "Users can build these as needed",
    
    examples: [
      {
        name: "waitlock-cleanup",
        purpose: "Remove stale locks",
        implementation: [
          "#!/bin/sh",
          "# Simple stale lock cleanup",
          "waitlock --list --stale-only --format=csv | \\",
          "  while IFS=, read desc pid rest; do",
          "    rm -f \"${WAITLOCK_DIR:-/var/run/waitlock}/${desc}.*.lock\"",
          "  done"
        ]
      },
      
      {
        name: "waitlock-monitor",
        purpose: "Monitor lock health",
        implementation: [
          "#!/bin/sh",
          "# Alert on long-held locks",
          "waitlock --list --format=csv | \\",
          "  awk -F, 'systime()-$4 > 300 {print $1 \" held too long\"}'"
        ]
      },
      
      {
        name: "waitlock-kill", 
        purpose: "Kill lock holders",
        implementation: [
          "#!/bin/sh",
          "# Kill processes holding a specific lock",
          "waitlock --list --format=csv | \\",
          "  awk -F, -v lock=\"$1\" '$1==lock {print $2}' | \\",
          "  xargs -r kill"
        ]
      }
    ]
  }
}
```

## Man Page Structure

```blueprint
ManPage {
  
  Synopsis {
    "waitlock [options] <descriptor>",
    "waitlock --list [--format=<fmt>] [--all|--stale-only]",
    "waitlock --check <descriptor>",
    "waitlock --exec <command> <descriptor>",
    "echo <descriptor> | waitlock [options]"
  },
  
  Description {
    brief: "waitlock - process synchronization tool for shell scripts",
    
    details: [
      "waitlock provides mutex and semaphore functionality for coordinating",
      "access to resources between multiple processes. Locks are automatically",
      "released when the process exits, ensuring no stale locks remain."
    ]
  },
  
  Examples {
    "Simple mutex:",
    "  waitlock db_backup && perform_backup",
    "",
    "Semaphore with 4 slots:",
    "  waitlock -m 4 render_job",
    "",
    "Execute with lock:",
    "  waitlock --exec 'rsync -av /src /dst' backup_lock",
    "",
    "Check and report:",
    "  waitlock --check busy_resource || echo 'Resource is locked'",
    "",
    "List active locks:",
    "  waitlock --list --format=csv | grep -c active"
  },
  
  SeeAlso: [
    "flock(1) - file-based locking",
    "lockfile(1) - simple file locking",
    "sem(1) - GNU parallel semaphore"
  ]
}
```

## Summary

WaitLock provides a simple, portable solution for process synchronization in shell scripts following UNIX philosophy:

- **Do One Thing Well**: Focus on acquiring and releasing locks
- **Composable**: Works in pipelines and with standard UNIX tools
- **Text Streams**: Multiple output formats for easy parsing
- **Environment Friendly**: Respects environment variables
- **Predictable**: Clear exit codes and behavior

By using proven UNIX mechanisms and careful C implementation, it achieves reliability across diverse platforms while maintaining simplicity. The tool fills a gap in standard UNIX utilities by providing both mutex and semaphore semantics with automatic cleanup, suitable for modern multi-core systems and distributed environments.
