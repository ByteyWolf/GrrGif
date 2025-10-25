#include <stdint.h>
#include "modules.h"

struct ModuleWindow windows[3] = {
    [POSITION_TOPLEFT] = { "Storyboard",  WINDOW_WIDTH * 0.3, WINDOW_HEIGHT * 0.6, POSITION_TOPLEFT },
    [POSITION_TOPRIGHT] = { "Preview",    WINDOW_WIDTH * 0.7, WINDOW_HEIGHT * 0.6, POSITION_TOPRIGHT },
    [POSITION_BOTTOM]   = { "Timeline",   WINDOW_WIDTH,        WINDOW_HEIGHT * 0.4, POSITION_BOTTOM }
};