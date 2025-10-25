#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "gif/gif.h"
#include "modules.h"
#include "text.h"

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

extern struct ModuleWindow windows[3];

void shrink_rect(SDL_FRect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->w -= pixels * 2;
    rect->h -= pixels * 2;
}

// Placeholder: Two simple animated GIFs to test rendering

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("GrrGif", "1.0", "com.byteywolf.grrgif");
    init_text_system(renderer);
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    if (!SDL_CreateWindowAndRenderer("GrrGif", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    
    // unnecessary because our ui adapts
    // SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // loading things but probs unnecessary for now
    
    img_wolf = parse("./tests/wolf.gif");
    if (!img_wolf) {
        SDL_Log("Failed to parse wolf image");
        return SDL_APP_FAILURE;
    }
    
    tex_wolf = SDL_CreateTexture(renderer,
                                  SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  img_wolf->width, img_wolf->height);
    
    img_banana = parse("./tests/dancing-banana-banana.gif");
    if (!img_banana) {
        SDL_Log("Failed to parse banana image");
        return SDL_APP_FAILURE;
    }
    
    tex_banana = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    img_banana->width, img_banana->height);
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    for (int winID = 0; winID < 3; winID++) {
        struct ModuleWindow current_window = windows[winID];
        SDL_FRect window_rect = {0, 0, current_window.width, current_window.height};
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
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderFillRect(renderer, &window_rect);
        shrink_rect(&window_rect, 2);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderFillRect(renderer, &window_rect);
        shrink_rect(&window_rect, 2);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &window_rect);
        shrink_rect(&window_rect, 2);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderFillRect(renderer, &window_rect);

        SDL_Color bg_color = {30, 30, 30, 200};
        draw_text_bg(current_window.title, window_rect.x + 10, window_rect.y + 10, bg_color);
        

    }
    SDL_RenderPresent(renderer);
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}