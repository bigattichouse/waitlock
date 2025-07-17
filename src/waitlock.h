#ifndef WAITLOCK_H
#define WAITLOCK_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pwd.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef HAVE_SYS_SELECT_H
  #include <sys/select.h>
#endif

#ifdef HAVE_STDINT_H
  #include <stdint.h>
#else
  typedef unsigned int uint32_t;
  typedef unsigned short uint16_t;
  typedef unsigned char uint8_t;
#endif

#ifdef HAVE_FLOCK
  #include <sys/file.h>
#else
  /* Define flock constants for fcntl fallback */
  #define LOCK_EX 2
  #define LOCK_NB 4
#endif

#ifdef HAVE_SYSLOG_H
  #include <syslog.h>
#endif

/* Define feature test macros for usleep */
#ifndef _BSD_SOURCE
  #define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE
#endif
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/* Constants */
#define VERSION "1.0.0"
#define MAX_DESC_LEN 255
#define MAX_HOSTNAME 256
#define MAX_CMDLINE 4096
#define LOCK_MAGIC 0x57414C4B  /* "WALK" */

#ifndef PATH_MAX
  #define PATH_MAX 4096
#endif

/* Exit codes */
#define E_SUCCESS    0
#define E_BUSY       1
#define E_TIMEOUT    2
#define E_USAGE      3
#define E_SYSTEM     4
#define E_NOPERM     5
#define E_NODIR      6
#define E_TEMPFAIL   75
#define E_EXEC       126
#define E_NOTFOUND   127

/* Boolean type for C89 */
typedef int bool;
#define TRUE 1
#define FALSE 0

/* Output formats */
typedef enum {
    FMT_HUMAN,
    FMT_CSV,
    FMT_NULL
} output_format_t;

/* Lock info structure */
struct lock_info {
    uint32_t magic;
    uint32_t version;
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    time_t acquired_at;
    uint16_t lock_type;  /* 0=mutex, 1=semaphore */
    uint16_t max_holders;
    uint16_t slot;       /* Semaphore slot number (0 to max_holders-1) */
    uint16_t reserved;   /* Reserved for future use */
    char hostname[MAX_HOSTNAME];
    char descriptor[MAX_DESC_LEN + 1];
    char cmdline[MAX_CMDLINE];
    uint32_t checksum;
};

/* Global state structure */
struct global_state {
    int lock_fd;
    char lock_path[PATH_MAX];
    volatile sig_atomic_t should_exit;
    bool quiet;
    bool verbose;
    bool use_syslog;
    int syslog_facility;
    volatile pid_t child_pid;  /* For signal forwarding in exec mode */
};

/* Command line options structure */
struct options {
    const char *descriptor;
    int max_holders;
    bool one_per_cpu;
    int exclude_cpus;
    double timeout;
    bool check_only;
    bool list_mode;
    bool done_mode;
    bool show_all;
    bool stale_only;
    output_format_t output_format;
    const char *lock_dir;
    char **exec_argv;
    bool test_mode;
    int preferred_slot;  /* Preferred slot number (-1 for auto) */
};

/* Global variables */
extern struct global_state g_state;
extern struct options opts;

/* Function prototypes from core module */
void usage(FILE *stream);
void version(void);
int parse_args(int argc, char *argv[]);
void debug(const char *fmt, ...);
void error(int code, const char *fmt, ...);
int safe_snprintf(char *buf, size_t size, const char *fmt, ...);

/* Function prototypes from lock module */
char* find_lock_directory(void);
int acquire_lock(const char *descriptor, int max_holders, double timeout);
void release_lock(void);
int check_lock(const char *descriptor);
int list_locks(output_format_t format, bool show_all, bool stale_only);
int done_lock(const char *descriptor);
int portable_lock(int fd, int operation);

/* Function prototypes from process module */
bool process_exists(pid_t pid);
char* get_process_cmdline(pid_t pid);
int exec_with_lock(const char *descriptor, char *argv[]);

/* Function prototypes from signal module */
void signal_handler(int sig);
void install_signal_handlers(void);

/* Function prototypes from checksum module */
uint32_t calculate_crc32(const void *data, size_t len);
uint32_t calculate_lock_checksum(const struct lock_info *info);
bool validate_lock_checksum(const struct lock_info *info);

/* C89 compatibility declarations */
#ifndef HAVE_GETHOSTNAME
int gethostname(char *name, size_t len);
#endif

#ifndef HAVE_USLEEP
int usleep(unsigned int usec);
#endif

#endif /* WAITLOCK_H */