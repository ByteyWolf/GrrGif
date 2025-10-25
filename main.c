#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "gif/gif.h"

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

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

// Placeholder: Two simple animated GIFs to test rendering

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("GrrGif", "1.0", "com.byteywolf.grrgif");
    
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    if (!SDL_CreateWindowAndRenderer("renderbase", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    
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
    uint64_t now = SDL_GetTicks();
    
    struct frame32* f_wolf = img_wolf->frames[crt_frame_wolf];
    if (last_tick_wolf == 0) {
        last_tick_wolf = now;
    }
    if (now - last_tick_wolf >= f_wolf->delay) {
        crt_frame_wolf = (crt_frame_wolf + 1) % img_wolf->frame_count;
        last_tick_wolf = now;
        f_wolf = img_wolf->frames[crt_frame_wolf];
    }
    
    struct frame32* f_banana = img_banana->frames[crt_frame_banana];
    if (last_tick_banana == 0) {
        last_tick_banana = now;
    }
    if (now - last_tick_banana >= f_banana->delay) {
        crt_frame_banana = (crt_frame_banana + 1) % img_banana->frame_count;
        last_tick_banana = now;
        f_banana = img_banana->frames[crt_frame_banana];
    }
    
    SDL_RenderClear(renderer);
    
    SDL_UpdateTexture(tex_wolf, NULL, f_wolf->pixels, img_wolf->width * sizeof(uint32_t));
    SDL_FRect wolf_rect = {
        (WINDOW_WIDTH - img_wolf->width) / 2.0f,
        50,
        (float)img_wolf->width,
        (float)img_wolf->height
    };
    SDL_RenderTexture(renderer, tex_wolf, NULL, &wolf_rect);
    
    SDL_UpdateTexture(tex_banana, NULL, f_banana->pixels, img_banana->width * sizeof(uint32_t));
    SDL_FRect banana_rect = {
        (WINDOW_WIDTH - img_banana->width) / 2.0f,
        wolf_rect.y + wolf_rect.h + 20,
        (float)img_banana->width,
        (float)img_banana->height
    };
    SDL_RenderTexture(renderer, tex_banana, NULL, &banana_rect);
    
    SDL_RenderPresent(renderer);
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}