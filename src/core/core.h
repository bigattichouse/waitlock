#ifndef WAITLOCK_CORE_H
#define WAITLOCK_CORE_H

#include "../waitlock.h"

/* C89 compatibility function for strcasecmp */
int strcasecmp_compat(const char *s1, const char *s2);

/* Syslog facility parsing */
int parse_syslog_facility(const char *facility_name);

/* Command line parsing and core utilities */
int parse_args(int argc, char *argv[]);
void usage(FILE *stream);
void version(void);
void debug(const char *fmt, ...);
void error(int code, const char *fmt, ...);
int safe_snprintf(char *buf, size_t size, const char *fmt, ...);

/* CPU count detection */
int get_cpu_count(void);

#endif /* WAITLOCK_CORE_H */