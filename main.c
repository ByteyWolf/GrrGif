#include "graphics/graphics.h"
#include "gif/gif.h"
#include "modules.h"
#include "modules/modfunc.h"
#include "timeline/timeline.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

struct image32* img_wolf = NULL;
struct image32* img_banana = NULL;

const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_GRAY = 0x151515;

extern uint32_t wwidth;
extern uint32_t wheight;

uint8_t pendingRedraw = 1;

extern struct ModuleWindow windows[3];

void shrink_rect(Rect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->width -= pixels * 2;
    rect->height -= pixels * 2;
}

int main(int argc, char *argv[]) {
    int running = 1;
    Event event;

    if (!init_graphics(WINDOW_WIDTH, WINDOW_HEIGHT)) {
        fprintf(stderr, "Failed to initialize graphics\n");
        return 1;
    }

    img_wolf = parse("./tests/wolf.gif");
    if (!img_wolf) printf("Failed to parse wolf.gif");
    img_banana = parse("./tests/dancing-banana-banana.gif");
    if (!img_banana) printf("Failed to parse banana.gif");

    struct LoadedFile* test1f = malloc(sizeof(struct LoadedFile));
    test1f->imagePtr = img_wolf;
    test1f->type = FILE_ANIMATION;

    struct TimelineObject* test1 = malloc(sizeof(struct TimelineObject));
    test1->x = 30;
    test1->y = 0;
    test1->width = 300;
    test1->height = 224;
    test1->effectsList = 0;
    test1->fileSource = 0;
    test1->timePosMs = 0;
    test1->length = 100;
    test1->metadata = test1f;

    insertTimelineObj(test1);

    // -------------------- Main loop --------------------
    
    while (running) {
        while (poll_event(&event)) {
            if (event.type == EVENT_QUIT) {running = 0; break;}
            if (event.type == EVENT_RESIZE) {
                // resize windows appropriately
                windows[POSITION_BOTTOM].width = wwidth;
                windows[POSITION_TOPLEFT].height = (wheight - windows[POSITION_BOTTOM].height);
                windows[POSITION_TOPRIGHT].height = (wheight - windows[POSITION_BOTTOM].height);
                windows[POSITION_TOPRIGHT].width = (wwidth - windows[POSITION_TOPLEFT].width);
                pendingRedraw = 1;
            }
        }

        //clear_graphics(0x000000);
        if (!pendingRedraw) continue;
        pendingRedraw = 0;
        
        for (int winID = 0; winID < 3; winID++) {
            struct ModuleWindow current_window = windows[winID];
            Rect window_rect = {0, 0, current_window.width, current_window.height};

            switch (current_window.position) {
                case POSITION_TOPLEFT:
                    window_rect.x = 0;
                    window_rect.y = 0;
                    break;
                case POSITION_TOPRIGHT:
                    window_rect.x = wwidth - current_window.width;
                    window_rect.y = 0;
                    break;
                case POSITION_BOTTOM:
                    window_rect.x = 0;
                    window_rect.y = wheight - current_window.height;
                    break;
            }
            draw_rect(&window_rect, 0x0A0A0A);
            shrink_rect(&window_rect, 2);
            draw_rect(&window_rect, COLOR_GRAY);
            shrink_rect(&window_rect, 5);
            draw_rect(&window_rect, 0x303030);
            shrink_rect(&window_rect, 2);
            draw_rect(&window_rect, COLOR_GRAY);

            set_font_size(FONT_SIZE_LARGE);
            draw_text_bg(current_window.title, window_rect.x + 10, window_rect.y + 11, COLOR_WHITE, COLOR_GRAY);
        }

        preview_draw(wwidth - windows[1].width, 0, windows[1].width, windows[1].height);

        flush_graphics();
    }

    close_graphics();
    return 0;
}
