/*
 * waitlock - Process synchronization tool for shell scripts
 * 
 * Copyright (c) 2024
 * 
 * Main entry point and command line handling.
 */

#include "waitlock.h"
#include "core/core.h"
#include "lock/lock.h"
#include "process/process.h"
#include "signal/signal.h"
#include "test/test.h"

/* Global state for signal handlers */
#ifdef HAVE_SYSLOG_H
struct global_state g_state = { -1, "", 0, FALSE, FALSE, FALSE, LOG_DAEMON, 0, 0, 0 };
#else
struct global_state g_state = { -1, "", 0, FALSE, FALSE, FALSE, 0, 0, 0, 0 };
#endif

/* Command line options */
struct options opts = {
    NULL,      /* descriptor */
    1,         /* max_holders */
    FALSE,     /* one_per_cpu */
    0,         /* exclude_cpus */
    -1.0,      /* timeout (infinite) */
    FALSE,     /* check_only */
    FALSE,     /* list_mode */
    FALSE,     /* done_mode */
    FALSE,     /* show_all */
    FALSE,     /* stale_only */
    FMT_HUMAN, /* output_format */
    NULL,      /* lock_dir */
    NULL,      /* exec_argv */
    FALSE,     /* test_mode */
    -1         /* preferred_slot (auto) */
};

/* Main function */
int main(int argc, char *argv[]) {
    int ret;
    char *env_debug;
    
    /* Initialize random number generator */
    srand(time(NULL) ^ getpid());
    
    /* Check environment variables */
    env_debug = getenv("WAITLOCK_DEBUG");
    if (env_debug && (strcmp(env_debug, "1") == 0 || 
                      strcasecmp_compat(env_debug, "true") == 0 ||
                      strcasecmp_compat(env_debug, "yes") == 0)) {
        g_state.verbose = TRUE;
    }
    
    /* Parse command line */
    ret = parse_args(argc, argv);
    if (ret != 0) {
        return ret;
    }
    
    /* Install signal handlers */
    install_signal_handlers();
    
    /* Handle different modes */
    if (opts.test_mode) {
        return run_unit_tests();
    }
    
    if (opts.list_mode) {
        return list_locks(opts.output_format, opts.show_all, opts.stale_only);
    }
    
    if (opts.check_only) {
        return check_lock(opts.descriptor);
    }
    
    if (opts.done_mode) {
        return done_lock(opts.descriptor);
    }
    
    if (opts.exec_argv) {
        return exec_with_lock(opts.descriptor, opts.exec_argv);
    }
    
    /* Normal lock acquisition */
    ret = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
    if (ret != 0) {
        return ret;
    }
    
    /* Wait for signal */
    while (!g_state.should_exit) {
        pause();
    }
    
    release_lock();
    return E_SUCCESS;
}