#ifndef WAITLOCK_PROCESS_H
#define WAITLOCK_PROCESS_H

#include "../waitlock.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <sys/sysctl.h>
#ifdef __FreeBSD__
#include <sys/user.h>
#endif
#endif

/* Process management functions */
bool process_exists(pid_t pid);
char* get_process_cmdline(pid_t pid);
int exec_with_lock(const char *descriptor, char *argv[]);

#endif /* WAITLOCK_PROCESS_H */