#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_Texture *font;
#define FIRST_CHAR 32
#define CHAR_WIDTH 6
#define CHAR_HEIGHT 12
#define Y_OFFSET 2
#define CHARS_PER_ROW 96/CHAR_WIDTH

SDL_Renderer *useRenderer;

void init_text_system(SDL_Renderer *renderer) {
    useRenderer = renderer;
    SDL_Surface *surf = SDL_LoadBMP("fonts/monogram-bitmap.bmp");
    SDL_Surface *font_surface = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);

    uint32_t *pixels = (uint32_t *)font_surface->pixels;
    int pixel_count = font_surface->w * font_surface->h;

    for (int i = 0; i < pixel_count; ++i) {
        uint8_t r, g, b, a;
        SDL_GetRGBA(pixels[i], font_surface->format, NULL, &r, &g, &b, &a);

        if (r == 255 && g == 255 && b == 255) {
            a = 0;
            pixels[i] = SDL_MapRGBA(font_surface->format, NULL, r, g, b, a);
        } else {
            pixels[i] = SDL_MapRGBA(font_surface->format, NULL, r, g, b, 255);
        }
    }


    font = SDL_CreateTextureFromSurface(renderer, font_surface);
    SDL_DestroySurface(font_surface);
}

void draw_char(char c, int x, int y) {
    if (c < FIRST_CHAR || c >= 96) return;

    int char_index = c - FIRST_CHAR;
    SDL_FRect src_rect = {
        .x = (char_index % CHARS_PER_ROW) * CHAR_WIDTH,
        .y = (char_index / CHARS_PER_ROW) * CHAR_HEIGHT + Y_OFFSET,
        .w = CHAR_WIDTH,
        .h = CHAR_HEIGHT
    };
    SDL_FRect dst_rect = {
        .x = x,
        .y = y,
        .w = CHAR_WIDTH,
        .h = CHAR_HEIGHT
    };

    SDL_RenderTexture(useRenderer, font, &src_rect, &dst_rect);   
}

void draw_text(const char *text, int x, int y) {
    int cursor_x = x;
    int cursor_y = y;

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y += CHAR_HEIGHT;
        } else {
            draw_char(*p, cursor_x, cursor_y);
            cursor_x += CHAR_WIDTH;
        }
    }
}

void draw_text_bg(const char *text, int x, int y, SDL_Color bg_color) {
    int text_length = 0;
    int line_count = 1;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            line_count++;
            text_length = 0;
        } else {
            text_length++;
        }
    }
    int bg_width = text_length * CHAR_WIDTH + 4;
    int bg_height = line_count * CHAR_HEIGHT + 4;

    SDL_SetRenderDrawColor(useRenderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_FRect bg_rect = { x - 2, y - 2, bg_width, bg_height };
    SDL_RenderFillRect(useRenderer, &bg_rect);

    draw_text(text, x, y);
}