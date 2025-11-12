#pragma once

#include <stdint.h>

typedef struct Rect {
    int x, y;
    int width, height;
} Rect;

typedef struct Event {
    uint8_t type;
    uint8_t pending;
    int x;
    int y;
    int key;
    int width;
    int height;
    int scrollDelta;
    uint32_t command;
} Event;

#define EVENT_NONE 0
#define EVENT_KEYDOWN 1
#define EVENT_KEYUP 2
#define EVENT_MOUSEMOVE 3
#define EVENT_MOUSEBUTTONDOWN 4
#define EVENT_MOUSEBUTTONUP 5
#define EVENT_QUIT 6
#define EVENT_RESIZE 7
#define EVENT_MOUSESCROLL 8
#define EVENT_COMMAND 9

#define FONT_SIZE_NORMAL 8
#define FONT_SIZE_LARGE 12

#define ANCHOR_LEFT 0
#define ANCHOR_MIDDLE 1
#define ANCHOR_RIGHT 2

#define CURSOR_NORMAL   0
#define CURSOR_BUSY     1
#define CURSOR_SIZEH    2
#define CURSOR_SIZEV    3
#define CURSOR_MOVE     4

#define COMMAND_FILE_EXIT 1
#define COMMAND_ACTION_ADDTRACK 2
#define COMMAND_HELP_ABOUT 3

#define MSGBOX_INFO 0
#define MSGBOX_WARNING 1
#define MSGBOX_ERROR 2
#define MSGBOX_QUESTION 3


int init_graphics(uint32_t width, uint32_t height, Event *event);
int draw_rect(Rect *rect, uint32_t color);
int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t x, uint32_t y);
int flush_graphics();
int close_graphics();
int clear_graphics(uint32_t color);
int poll_event();
//int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height);

uint8_t create_menu();
void append_menu(uint8_t handle, char* name, uint32_t code);
void append_menu_separator(uint8_t handle);
void finalize_menu(uint8_t handle, char* name);


int draw_text(const char *text, int x, int y, uint32_t color);
int draw_text_anchor(char *text, int x, int y, uint32_t color, uint8_t anchor);
int draw_text_bg(const char *text, int x, int y, uint32_t fg_color, uint32_t bg_color);
int draw_text_limited(char *text, int x, int y, uint32_t color, uint8_t anchor, uint32_t max_width);
int set_font_size(int font_size);

void set_window_title(char *title);
void set_cursor(int type);

void shrink_rect(Rect* rect, int pixels);

char* choose_file();
int messagebox(char* title, char* body, int type);

void preview_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void timeline_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void timeline_scroll(int delta);
void preview_handle_event(Event* event);

