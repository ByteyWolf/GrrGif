#include <SDL2/SDL.h>
#include "text.h"

SDL_Texture *font = NULL;
SDL_Renderer *useRenderer = NULL;

#define FIRST_CHAR 32
#define LAST_CHAR 127
#define CHAR_COUNT (LAST_CHAR - FIRST_CHAR + 1)
#define BCHAR_WIDTH 6
#define BCHAR_HEIGHT 12
#define Y_OFFSET 2
#define CHARS_PER_ROW 16

static int current_font_size = FONT_NORMAL;
static SDL_Color current_text_color = {255, 255, 255, 255};

void init_text_system(SDL_Renderer *renderer) {
    useRenderer = renderer;
    SDL_Surface *surf = SDL_LoadBMP("fonts/monogram-bitmap.bmp");
    if (!surf) {
        SDL_Log("Failed to load font BMP: %s", SDL_GetError());
        return;
    }
    
    SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, 0, 0, 0));
    font = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    
    if (font) {
        SDL_SetTextureBlendMode(font, SDL_BLENDMODE_BLEND);
    }
}

void cleanup_text_system(void) {
    if (font) {
        SDL_DestroyTexture(font);
        font = NULL;
    }
}

void set_font_size(int size) {
    if (size == FONT_NORMAL || size == FONT_LARGE) {
        current_font_size = size;
    }
}

void set_text_color(SDL_Color color) {
    current_text_color = color;
}

void set_text_color_rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    current_text_color.r = r;
    current_text_color.g = g;
    current_text_color.b = b;
    current_text_color.a = a;
}

void draw_char(char c, int x, int y) {
    if (!font || c < FIRST_CHAR || c > LAST_CHAR) return;
    
    int char_index = c - FIRST_CHAR;
    SDL_Rect src_rect = {
        .x = (char_index % CHARS_PER_ROW) * BCHAR_WIDTH,
        .y = (char_index / CHARS_PER_ROW) * BCHAR_HEIGHT + Y_OFFSET,
        .w = BCHAR_WIDTH,
        .h = BCHAR_HEIGHT
    };
    SDL_Rect dst_rect = { 
        x, 
        y, 
        BCHAR_WIDTH * current_font_size, 
        BCHAR_HEIGHT * current_font_size
    };
    
    SDL_SetTextureColorMod(font, current_text_color.r, current_text_color.g, current_text_color.b);
    SDL_SetTextureAlphaMod(font, current_text_color.a);
    
    SDL_RenderCopy(useRenderer, font, &src_rect, &dst_rect);
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