/*
 * Signal handling functions
 */

#include "signal.h"

/* Use simple signal() for C89 compatibility */
#include <signal.h>

/* Signal handler - only signal-safe operations */
void signal_handler(int sig) {
    /* If we have a child process (exec mode), forward the signal to it */
    if (g_state.child_pid > 0) {
        /* Forward signal to child process (signal-safe) */
        kill(g_state.child_pid, sig);
        
        /* Set flag to indicate we should exit after child terminates */
        g_state.should_exit = 1;
        g_state.received_signal = sig;
        return;
    }
    
    /* Normal signal handling when not in exec mode */
    g_state.should_exit = 1;
    g_state.received_signal = sig;
    
    /* Only perform minimal signal-safe cleanup */
    if (g_state.lock_fd >= 0) {
        close(g_state.lock_fd);
        g_state.lock_fd = -1;
    }
    
    /* Mark for cleanup but don't do unsafe operations in signal handler */
    g_state.cleanup_needed = 1;
    
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