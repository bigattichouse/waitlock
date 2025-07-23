AC_INIT([waitlock], [1.0.0], [support@waitlock.org])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CC_C89
AC_PROG_INSTALL

# Enable more warnings
if test "$GCC" = "yes"; then
    CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter"
fi

# Check for standard headers
AC_CHECK_HEADERS([sys/file.h sys/select.h syslog.h stdint.h])
AC_CHECK_HEADERS([sys/types.h sys/stat.h sys/wait.h sys/time.h])
AC_CHECK_HEADERS([sys/sysctl.h sys/user.h stdbool.h])
AC_CHECK_HEADERS([signal.h pwd.h dirent.h limits.h ctype.h])
AC_CHECK_HEADERS([string.h unistd.h fcntl.h errno.h time.h])

# Check for BSD/macOS specific headers
AC_CHECK_HEADERS([sys/param.h sys/mount.h sys/vfs.h])

# Check for essential functions
AC_CHECK_FUNCS([flock fcntl lockf])
AC_CHECK_FUNCS([snprintf vsnprintf strcasecmp])
AC_CHECK_FUNCS([sysconf usleep])
AC_CHECK_FUNCS([kill signal])
AC_CHECK_FUNCS([fork execvp waitpid])
AC_CHECK_FUNCS([getpid getppid getuid getpwuid])
AC_CHECK_FUNCS([opendir readdir closedir])
AC_CHECK_FUNCS([gethostname])
AC_CHECK_FUNCS([sysctl sysctlbyname])

# Check for library functions
AC_CHECK_FUNCS([openlog syslog closelog])

# Check for declarations
AC_CHECK_DECLS([_SC_NPROCESSORS_ONLN], [], [], [[#include <unistd.h>]])

# Check for syslog facilities
AC_CHECK_DECLS([LOG_DAEMON, LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7], [], [], [[#include <syslog.h>]])

# Check for BSD sysctl constants
AC_CHECK_DECLS([CTL_KERN, KERN_PROC, KERN_PROC_PID], [], [], [[#include <sys/sysctl.h>]])

# Check for platform-specific features
AC_CHECK_MEMBERS([struct kinfo_proc.kp_proc.p_comm, struct kinfo_proc.ki_comm], [], [], [[#include <sys/sysctl.h>]])

# Check for types
AC_CHECK_TYPES([pid_t, size_t, ssize_t])

# Platform detection
case $host_os in
    freebsd*)
        AC_DEFINE([HAVE_FREEBSD], [1], [Define if building on FreeBSD])
        ;;
    openbsd*)
        AC_DEFINE([HAVE_OPENBSD], [1], [Define if building on OpenBSD])
        ;;
    netbsd*)
        AC_DEFINE([HAVE_NETBSD], [1], [Define if building on NetBSD])
        ;;
    darwin*)
        AC_DEFINE([HAVE_MACOS], [1], [Define if building on macOS])
        ;;
    linux*)
        AC_DEFINE([HAVE_LINUX], [1], [Define if building on Linux])
        ;;
esac

# Feature macros
AC_DEFINE([_GNU_SOURCE], [1], [Enable GNU extensions])
AC_DEFINE([_BSD_SOURCE], [1], [Enable BSD extensions])
AC_DEFINE([_DEFAULT_SOURCE], [1], [Enable default source])
AC_DEFINE([_POSIX_C_SOURCE], [200112L], [Enable POSIX.1-2001 features])

# Output files
AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
