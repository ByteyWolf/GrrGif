#pragma once

typedef struct Rect {
    int x, y;
    int width, height;
} Rect;

typedef struct Event {
    int type;
    int x;
    int y;
    int key;
} Event;

#define EVENT_NONE 0
#define EVENT_KEYDOWN 1
#define EVENT_KEYUP 2
#define EVENT_MOUSEMOVE 3
#define EVENT_MOUSEBUTTONDOWN 4
#define EVENT_MOUSEBUTTONUP 5
#define EVENT_QUIT 6


int init_graphics(uint32_t width, uint32_t height);
int draw_rect(Rect *rect, uint32_t color);
int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height);
int flush_graphics();
int close_graphics();
int clear_graphics(uint32_t color);
Event poll_event();
int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height);