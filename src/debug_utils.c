#include "debug_utils.h"
#include "waitlock.h" // For g_state definition

void debug(const char *format, ...) {
    if (g_state.verbose) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

