#include "graphics/graphics.h"
#include "gif/gif.h"
#include "modules.h"
#include "modules/modfunc.h"
#include "timeline/timeline.h"
#include "debug.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

#define DRAG_BORDER_NONE 0
#define DRAG_BORDER_X 1
#define DRAG_BORDER_Y 2

struct image32* img_wolf = NULL;
struct image32* img_banana = NULL;

const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_GRAY = 0x151515;

extern uint32_t wwidth;
extern uint32_t wheight;

uint8_t pendingRedraw = 1;
uint8_t eligibleToDragBorder = DRAG_BORDER_NONE;
uint8_t draggingWindowBorder = DRAG_BORDER_NONE;
uint32_t initialPos = 0;

extern struct ModuleWindow windows[3];

void shrink_rect(Rect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->width -= pixels * 2;
    rect->height -= pixels * 2;
}

int main(int argc, char *argv[]) {
    printf("GrrGif v0.0\n(c) Copyright 2025 Bytey Wolf. All rights reserved.\n\n");

    int running = 1;
    Event event;

    debugf(DEBUG_INFO, "Initializing graphics system...");

    if (!init_graphics(WINDOW_WIDTH, WINDOW_HEIGHT)) {
        fprintf(stderr, "Failed to initialize graphics\n");
        return 1;
    }

    debugf(DEBUG_INFO, "Loading sample GIFs...");

    img_wolf = parse("./tests/wolf.gif");
    if (!img_wolf) printf("Failed to parse wolf.gif");
    img_banana = parse("./tests/dancing-banana-banana.gif");
    if (!img_banana) printf("Failed to parse banana.gif");

    debugf(DEBUG_VERBOSE, "Adding sample GIF to timeline.");

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
            switch (event.type) {
                case EVENT_RESIZE:
                    windows[POSITION_BOTTOM].width = wwidth;
                    windows[POSITION_TOPLEFT].height = (wheight - windows[POSITION_BOTTOM].height);
                    windows[POSITION_TOPRIGHT].height = (wheight - windows[POSITION_BOTTOM].height);
                    windows[POSITION_TOPRIGHT].width = (wwidth - windows[POSITION_TOPLEFT].width);
                    pendingRedraw = 1;
                    break;
                case EVENT_MOUSEMOVE:
                    if (draggingWindowBorder == DRAG_BORDER_NONE) {
                        int rightEdge = windows[POSITION_TOPLEFT].width;
                        int bottomEdge = windows[POSITION_TOPLEFT].height;

                        if (event.x >= rightEdge - 4 && event.x <= rightEdge + 4) {
                            set_cursor(CURSOR_SIZEH);
                            eligibleToDragBorder = DRAG_BORDER_X;
                        } else if (event.y >= bottomEdge - 4 && event.y <= bottomEdge + 4) {
                            set_cursor(CURSOR_SIZEV);
                            eligibleToDragBorder = DRAG_BORDER_Y;
                        } else {
                            set_cursor(CURSOR_NORMAL);
                            eligibleToDragBorder = DRAG_BORDER_NONE;
                        }
                    }
                    // oh we're dragging something
                    else {
                        int delta;
                        switch (draggingWindowBorder) {
                            case DRAG_BORDER_X:
                                delta = event.x - initialPos;
                                windows[POSITION_TOPLEFT].width += delta;
                                windows[POSITION_TOPRIGHT].width -= delta;
                                initialPos = event.x;
                                break;
                            case DRAG_BORDER_Y:
                                delta = event.y - initialPos;
                                windows[POSITION_TOPLEFT].height += delta;
                                windows[POSITION_TOPRIGHT].height += delta;
                                windows[POSITION_BOTTOM].height -= delta;
                                initialPos = event.y;
                                break;
                        }
                        pendingRedraw = 1;
                    }
                    break;
                case EVENT_MOUSEBUTTONDOWN:
                    draggingWindowBorder = eligibleToDragBorder;
                    switch (draggingWindowBorder) {
                        case DRAG_BORDER_X:
                            initialPos = event.x;
                            break;
                        case DRAG_BORDER_Y:
                            initialPos = event.y;
                            break;
                    }
                    break;
                case EVENT_MOUSEBUTTONUP:
                    draggingWindowBorder = DRAG_BORDER_NONE;
                    break;
            }
        }

        //clear_graphics(0x000000);
        if (!pendingRedraw) continue;
        pendingRedraw = 0;
        debugf(DEBUG_VERBOSE, "A redraw is pending.");
        
        for (int winID = 0; winID < 3; winID++) {
            debugf(DEBUG_VERBOSE, "Redrawing widget %u...", winID);
            struct ModuleWindow current_window = windows[winID];
            Rect window_rect;
            window_rect.x = 0;
            window_rect.y = 0;
            window_rect.width = current_window.width;
            window_rect.height = current_window.height;

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

    debugf(DEBUG_INFO, "Bye-bye...");
    close_graphics();
    return 0;
}
