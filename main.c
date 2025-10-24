#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "gif/gif.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture* tex = NULL;

static uint32_t crt_frame = 0;
static uint64_t last_tick = 0;

struct image32* img = NULL;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

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

    // test
    img = parse("./tests/wolf.gif");
    if (!img) {
        SDL_Log("Failed to parse image");
        return SDL_APP_FAILURE;
    }
    struct frame32* f = img->frames[0];
    tex = SDL_CreateTexture(renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     img->width, img->height);


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

    if (crt_frame < img->frame_count) {
        struct frame32* f = img->frames[crt_frame];
        if (now - last_tick >= f->delay * 10) {
            SDL_UpdateTexture(tex, NULL, f->pixels, img->width * sizeof(uint32_t));
            last_tick = now;
            crt_frame = (crt_frame + 1) % img->frame_count;
        }
    }

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, tex, NULL, NULL);
    SDL_RenderPresent(renderer);



    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{

}