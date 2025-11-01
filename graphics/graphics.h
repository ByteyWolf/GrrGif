#pragma once

#include <stdint.h>

typedef struct Rect {
    int x, y;
    int width, height;
} Rect;

typedef struct Event {
    int type;
    int x;
    int y;
    int key;
    int width;
    int height;
} Event;

#define EVENT_NONE 0
#define EVENT_KEYDOWN 1
#define EVENT_KEYUP 2
#define EVENT_MOUSEMOVE 3
#define EVENT_MOUSEBUTTONDOWN 4
#define EVENT_MOUSEBUTTONUP 5
#define EVENT_QUIT 6
#define EVENT_RESIZE 7

#define FONT_SIZE_NORMAL 8
#define FONT_SIZE_LARGE 12


int init_graphics(uint32_t width, uint32_t height);
int draw_rect(Rect *rect, uint32_t color);
int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t x, uint32_t y);
int flush_graphics();
int close_graphics();
int clear_graphics(uint32_t color);
int poll_event(Event *event);
//int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height);

int draw_text(const char *text, int x, int y, uint32_t color);
int draw_text_bg(const char *text, int x, int y, uint32_t fg_color, uint32_t bg_color);
int set_font_size(int font_size);
