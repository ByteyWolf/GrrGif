#include <SDL2/SDL.h>
#include "gif/gif.h"
#include "modules.h"
#include "text.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture* tex_wolf = NULL;
static SDL_Texture* tex_banana = NULL;
static uint32_t crt_frame_wolf = 0;
static uint32_t crt_frame_banana = 0;
static uint64_t last_tick_wolf = 0;
static uint64_t last_tick_banana = 0;
struct image32* img_wolf = NULL;
struct image32* img_banana = NULL;

const SDL_Color COLOR_WHITE = {255, 255, 255, 255};
const SDL_Color COLOR_GRAY = {20, 20, 20, 255};

extern struct ModuleWindow windows[3];

void shrink_rect(SDL_Rect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->w -= pixels * 2;
    rect->h -= pixels * 2;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("GrrGif", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    init_text_system(renderer);

    img_wolf = parse("./tests/wolf.gif");
    if (!img_wolf) SDL_Log("Failed to parse wolf.gif");
    else {
        tex_wolf = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     img_wolf->width, img_wolf->height);
    }

    img_banana = parse("./tests/dancing-banana-banana.gif");
    if (!img_banana) SDL_Log("Failed to parse banana.gif");
    else {
        tex_banana = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                       img_banana->width, img_banana->height);
    }

    // -------------------- Main loop --------------------
    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int winID = 0; winID < 3; winID++) {
            struct ModuleWindow current_window = windows[winID];
            SDL_Rect window_rect = {0, 0, current_window.width, current_window.height};

            switch (current_window.position) {
                case POSITION_TOPLEFT:
                    window_rect.x = 0;
                    window_rect.y = 0;
                    break;
                case POSITION_TOPRIGHT:
                    window_rect.x = WINDOW_WIDTH - current_window.width;
                    window_rect.y = 0;
                    break;
                case POSITION_BOTTOM:
                    window_rect.x = 0;
                    window_rect.y = WINDOW_HEIGHT - current_window.height;
                    break;
            }

            SDL_SetRenderDrawColor(renderer, 10,10,10,255);
            SDL_RenderFillRect(renderer, &window_rect);
            shrink_rect(&window_rect, 2);
            SDL_SetRenderDrawColor(renderer, 20,20,20,255);
            SDL_RenderFillRect(renderer, &window_rect);
            shrink_rect(&window_rect, 5);
            SDL_SetRenderDrawColor(renderer, 50,50,50,255);
            SDL_RenderFillRect(renderer, &window_rect);
            shrink_rect(&window_rect, 2);
            SDL_SetRenderDrawColor(renderer, 20,20,20,255);
            SDL_RenderFillRect(renderer, &window_rect);

            set_text_color(COLOR_WHITE);
            set_font_size(FONT_LARGE);
            draw_text_bg(current_window.title, window_rect.x + 10, window_rect.y, COLOR_GRAY);
        }

        SDL_RenderPresent(renderer);
    }

    if (tex_wolf) SDL_DestroyTexture(tex_wolf);
    if (tex_banana) SDL_DestroyTexture(tex_banana);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    cleanup_text_system();
    return 0;
}
