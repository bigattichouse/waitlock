#ifndef WAITLOCK_SIGNAL_H
#define WAITLOCK_SIGNAL_H

#include "../waitlock.h"

/* Signal handling functions */
void signal_handler(int sig);
void install_signal_handlers(void);

#endif /* WAITLOCK_SIGNAL_H */