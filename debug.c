#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

#ifdef _WIN32
#include <windows.h>
#endif

void debug_set_color(int priority) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    int color = 7;
    if (priority == DEBUG_ACTION) {
        color = 2;
    } else if (priority == DEBUG_INFO) {
        color = 6;
    } else if (priority == DEBUG_VERBOSE) {
        color = 3;
    }
    SetConsoleTextAttribute(hConsole, color);
#else
    if (priority == DEBUG_ACTION) {
        printf("\x1b[32m");
    } else if (priority == DEBUG_INFO) {
        printf("\x1b[33m");
    } else if (priority == DEBUG_VERBOSE) {
        printf("\x1b[36m");
    } else {
        printf("\x1b[0m");
    }
#endif
}
static void debug_reset_color(void) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 7);
#else
    printf("\x1b[0m");
#endif
}

const int debug_threshold = DEBUG_VERBOSE;
const char* debug_tags[] = {"DEBUG", "INFO", "OK"};

void debugf(int priority, const char *fmt, ...) {
    if (priority < debug_threshold) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    putchar('[');
    debug_set_color(priority);
    printf(debug_tags[priority]);
    debug_reset_color();
    putchar(']');
    putchar(' ');


    vprintf(fmt, args); 
    va_end(args);

    putchar('\n');
}