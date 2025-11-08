#pragma once

#define POSITION_TOPLEFT 0
#define POSITION_TOPRIGHT 1 // main
#define POSITION_BOTTOM 2

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

struct ModuleWindow {
    char title[256];
    int width;
    int height;
    int position;
};


