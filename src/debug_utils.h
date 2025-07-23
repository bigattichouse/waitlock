#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <stdio.h>
#include <stdarg.h>

// Forward declaration of global_state to access verbose flag
struct global_state;
extern struct global_state g_state;

void debug(const char *format, ...);

#endif // DEBUG_UTILS_H
