#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

void init_text_system(SDL_Renderer *renderer);
void draw_char(char c, int x, int y);
void draw_text(const char *text, int x, int y);
void draw_text_bg(const char *text, int x, int y, SDL_Color bg_color);

void cleanup_text_system(void);
void set_font_size(int size);
void set_text_color(SDL_Color color);
void set_text_color_rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

#define FONT_NORMAL 1
#define FONT_LARGE 2