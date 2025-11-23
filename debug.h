#pragma once

#define DEBUG_VERBOSE 0
#define DEBUG_INFO 1
#define DEBUG_ACTION 2


#include <stdio.h>

void debugf(int priority, const char *fmt, ...);

