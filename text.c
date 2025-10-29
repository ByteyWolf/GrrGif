#include "text.h"
#include "graphics/graphics.h"
#include "fonts/monogram.h"

#include <stdint.h>

#define FIRST_CHAR 32
#define LAST_CHAR 127
#define CHAR_COUNT (LAST_CHAR - FIRST_CHAR + 1)
#define BCHAR_WIDTH 6
#define BCHAR_HEIGHT 12
#define Y_OFFSET 2
#define CHARS_PER_ROW 16

static int current_font_size = FONT_NORMAL;
static current_text_color = 0xFFFFFF;

void set_font_size(int size) {
    if (size == FONT_NORMAL || size == FONT_LARGE) {
        current_font_size = size;
    }
}

void set_text_color(uint32_t color) {
    current_text_color = color;
}

void set_text_color_rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    current_text_color.r = r;
    current_text_color.g = g;
    current_text_color.b = b;
    current_text_color.a = a;
}

void draw_char(char c, int x, int y) {
    int char_index;
    Rect src_rect = {
        (char_index % CHARS_PER_ROW) * BCHAR_WIDTH,
        (char_index / CHARS_PER_ROW) * BCHAR_HEIGHT + Y_OFFSET,
        BCHAR_WIDTH,
        BCHAR_HEIGHT
    };
    Rect dst_rect = { 
        x, 
        y, 
        BCHAR_WIDTH * current_font_size, 
        BCHAR_HEIGHT * current_font_size
    };
    uint32_t* pixels = malloc(dst_rect.width * dst_rect.height);


    if (!get_rgb8888(pixels, dst_rect.width, dst_rect.height)) { fprintf(stderr, "Failed to get data from screen"); return;
    if (c < FIRST_CHAR || c > LAST_CHAR) return;
    
    for (uint32_t glyph_y = 0; glyph_y < BCHAR_HEIGHT; glyph_y++) {
        
    }
    
}

void draw_char_colored(char c, int x, int y, SDL_Color color) {
    SDL_Color old_color = current_text_color;
    set_text_color(color);
    draw_char(c, x, y);
    current_text_color = old_color;
}

void draw_text(const char *text, int x, int y) {
    if (!text) return;
    
    int cursor_x = x;
    int cursor_y = y;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y += BCHAR_HEIGHT * current_font_size;
        } else {
            draw_char(*p, cursor_x, cursor_y);
            cursor_x += BCHAR_WIDTH * current_font_size;
        }
    }
}


void draw_text_bg(const char *text, int x, int y, SDL_Color bg_color) {
    if (!text) return;
    
    int max_width = 0;
    int current_width = 0;
    int line_count = 1;
    
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            if (current_width > max_width) {
                max_width = current_width;
            }
            current_width = 0;
            line_count++;
        } else {
            current_width++;
        }
    }
    if (current_width > max_width) {
        max_width = current_width;
    }
    
    int bg_width = max_width * BCHAR_WIDTH * current_font_size + 4;
    int bg_height = line_count * BCHAR_HEIGHT * current_font_size + 4;
    
    SDL_SetRenderDrawColor(useRenderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_Rect bg_rect = { x - 2, y - 2, bg_width, bg_height };
    SDL_RenderFillRect(useRenderer, &bg_rect);
    
    draw_text(text, x, y);
}