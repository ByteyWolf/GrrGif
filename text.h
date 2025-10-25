#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

void init_text_system(SDL_Renderer *renderer);
void draw_char(char c, int x, int y);
void draw_text(const char *text, int x, int y);
void draw_text_bg(const char *text, int x, int y, SDL_Color bg_color);