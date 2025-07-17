/*
 * Signal handling functions
 */

#include "signal.h"

/* Use simple signal() for C89 compatibility */
#include <signal.h>

/* Signal handler */
void signal_handler(int sig) {
    /* If we have a child process (exec mode), forward the signal to it */
    if (g_state.child_pid > 0) {
        /* Forward signal to child process */
        if (kill(g_state.child_pid, sig) == 0) {
            /* Log signal forwarding to syslog if enabled */
            if (g_state.use_syslog) {
#ifdef HAVE_SYSLOG_H
                openlog("waitlock", LOG_PID, g_state.syslog_facility);
                syslog(LOG_INFO, "forwarded signal %d to child process %d", 
                       sig, (int)g_state.child_pid);
                closelog();
#endif
            }
        }
        
        /* Set flag to indicate we should exit after child terminates */
        g_state.should_exit = 1;
        return;
    }
    
    /* Normal signal handling when not in exec mode */
    g_state.should_exit = 1;
    
    /* Attempt cleanup */
    if (g_state.lock_fd >= 0) {
        close(g_state.lock_fd);
        g_state.lock_fd = -1;
    }
    
    if (g_state.lock_path[0]) {
        unlink(g_state.lock_path);
    }
    
    /* Re-raise signal with proper exit code */
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Install signal handlers */
void install_signal_handlers(void) {
    /* Use simple signal() for C89 compatibility */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}