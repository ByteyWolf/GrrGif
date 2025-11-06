#include "graphics/graphics.h"
#include "gif/gif.h"
#include "modules.h"
#include "modules/modfunc.h"
#include "timeline/timeline.h"
#include "debug.h"
#include "graphics/gui_utility.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_WINDOW_WIDTH 800
#define DEFAULT_WINDOW_HEIGHT 600

#define DRAG_BORDER_NONE 0
#define DRAG_BORDER_X 1
#define DRAG_BORDER_Y 2

struct imageV2* img_wolf = NULL;
struct imageV2* img_banana = NULL;

const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_GRAY = 0x151515;

extern uint32_t wwidth;
extern uint32_t wheight;

extern uint8_t previewPlaying;

uint8_t pendingRedraw = 1;
uint8_t eligibleToDragBorder = DRAG_BORDER_NONE;
uint8_t draggingWindowBorder = DRAG_BORDER_NONE;
uint32_t initialPos = 0;

extern struct ModuleWindow windows[3];
extern struct GUIButton* buttonBase;
extern struct GUIButton* crtButtonHovering;
extern struct GUIButton* crtButtonHeld;

void shrink_rect(Rect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->width -= pixels * 2;
    rect->height -= pixels * 2;
}

void hoverLogic(struct GUIButton* crtButton, Event* event) {
    if (!crtButton) {debugf(DEBUG_INFO, "Button hover error"); return;}
    /*if (crtButtonHeld) {
        drawButton(crtButton, BUTTON_STATE_NORMAL);
        drawButton(crtButtonHeld, BUTTON_STATE_CLICK);
        printf("held\n");
    }*/
            
    uint32_t left = getWindowX(crtButton->weldToWindow) + crtButton->localX;
    uint32_t top = getWindowY(crtButton->weldToWindow) + crtButton->localY;
    if (event->x >= left
            && event->x <= left + crtButton->width
            && event->y >= top
            && event->y <= top + crtButton->height) {
        setButtonState(crtButton, BUTTON_STATE_HOVER);
    } else {
        setButtonState(crtButton, BUTTON_STATE_NORMAL);
    }
    pendingRedraw = 1;
}

void renderUI() {
    while (1) {
        Sleep(10);
        timeline_heartbeat();
        if (!pendingRedraw) continue;
        pendingRedraw = 0;
        
        for (int winID = 0; winID < 3; winID++) {
            //debugf(DEBUG_VERBOSE, "Redrawing widget %u...", winID);
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
            draw_text_bg(current_window.title, window_rect.x + 10, window_rect.y - 4, COLOR_WHITE, COLOR_GRAY);
        }
    
        preview_draw(wwidth - windows[POSITION_TOPRIGHT].width, 0, windows[POSITION_TOPRIGHT].width, windows[POSITION_TOPRIGHT].height);
        timeline_draw(0, wheight - windows[POSITION_BOTTOM].height, windows[POSITION_BOTTOM].width, windows[POSITION_BOTTOM].height);
    
        struct GUIButton* crtButton = buttonBase;
        while (crtButton) {
            drawButton(crtButton);
            crtButton = crtButton->nextButton;
        }
    
        flush_graphics();
    }
}

#ifdef _WIN32
DWORD WINAPI BackgroundThread(LPVOID lpParam) {
    renderUI();
    return 0;
}

void scheduleRendering() {
    HANDLE hThread;
    DWORD dwThreadId;

    hThread = CreateThread(NULL, 0, BackgroundThread, NULL, 0, &dwThreadId);
    if (hThread == NULL) { printf("ERROR making bg thread!\n"); return;}
    CloseHandle(hThread);
}
#endif

int main(int argc, char *argv[]) {
    printf("GrrGif v0.0\n(c) Copyright 2025 Bytey Wolf. All rights reserved.\n\n");

    int running = 1;
    Event* event = malloc(sizeof(Event));

    debugf(DEBUG_INFO, "Initializing graphics system...");

    if (!init_graphics(WINDOW_WIDTH, WINDOW_HEIGHT, event)) {
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
    test1->length = 600;
    test1->metadata = test1f;
    test1->track = 0;

    insertTimelineObj(test1);
    scheduleRendering();
    
    while (running) {

        while (poll_event()) {
            if (event->type == EVENT_QUIT) {running = 0; break;}
            
            switch (event->type) {
                case EVENT_RESIZE:
                    windows[POSITION_BOTTOM].width = wwidth;
                    windows[POSITION_TOPLEFT].height = (wheight - windows[POSITION_BOTTOM].height);
                    windows[POSITION_TOPRIGHT].height = (wheight - windows[POSITION_BOTTOM].height);
                    windows[POSITION_TOPRIGHT].width = (wwidth - windows[POSITION_TOPLEFT].width);
                    pendingRedraw = 1;
                    break;
                case EVENT_MOUSEMOVE: {
                    // respond to buttons
                    struct GUIButton* crtButton = buttonBase;
                    while (crtButton && !crtButtonHeld) {
                        hoverLogic(crtButton, event);
                        crtButton = crtButton->nextButton;
                    }
                    if (draggingWindowBorder == DRAG_BORDER_NONE) {
                        int rightEdge = windows[POSITION_TOPLEFT].width;
                        int bottomEdge = windows[POSITION_TOPLEFT].height;

                        if (event->x >= rightEdge - 4 && event->x <= rightEdge + 4 && event->y < bottomEdge) {
                            set_cursor(CURSOR_SIZEH);
                            eligibleToDragBorder = DRAG_BORDER_X;
                        } else if (event->y >= bottomEdge - 4 && event->y <= bottomEdge + 4) {
                            set_cursor(CURSOR_SIZEV);
                            eligibleToDragBorder = DRAG_BORDER_Y;
                        } else {
                            set_cursor(CURSOR_NORMAL);
                            eligibleToDragBorder = DRAG_BORDER_NONE;
                        }
                    }
                    // oh we're dragging something
                    else {
                        pendingRedraw = 1;
                        int delta;
                        switch (draggingWindowBorder) {
                            case DRAG_BORDER_X:
                                delta = event->x - initialPos;
                                windows[POSITION_TOPLEFT].width += delta;
                                windows[POSITION_TOPRIGHT].width -= delta;
                                initialPos = event->x;
                                break;
                            case DRAG_BORDER_Y:
                                delta = event->y - initialPos;
                                windows[POSITION_TOPLEFT].height += delta;
                                windows[POSITION_TOPRIGHT].height += delta;
                                windows[POSITION_BOTTOM].height -= delta;
                                initialPos = event->y;
                                break;
                        }
                    }
                    break;
                }
                case EVENT_MOUSEBUTTONDOWN:
                    draggingWindowBorder = eligibleToDragBorder;
                    switch (draggingWindowBorder) {
                        case DRAG_BORDER_X:
                            initialPos = event->x;
                            break;
                        case DRAG_BORDER_Y:
                            initialPos = event->y;
                            break;
                    }
                    if (crtButtonHovering && !crtButtonHeld) {
                        setButtonState(crtButtonHovering, BUTTON_STATE_CLICK);
                        buttonCallback(crtButtonHeld);
                    }
                    break;
                case EVENT_MOUSEBUTTONUP:
                    draggingWindowBorder = DRAG_BORDER_NONE;
                    //setButtonState(crtButtonHeld, BUTTON_STATE_NORMAL);
                    if (crtButtonHeld)  hoverLogic(crtButtonHeld, event);
                    
                    break;
                case EVENT_MOUSESCROLL:
                    if (event->y > windows[POSITION_TOPLEFT].height) {
                        timeline_scroll(event->scrollDelta);
                    }
            }
        }

        //clear_graphics(0x000000);
        if (previewPlaying) preview_draw(wwidth - windows[POSITION_TOPRIGHT].width, 0, windows[POSITION_TOPRIGHT].width, windows[POSITION_TOPRIGHT].height);
        //renderUI();
        
    }

    debugf(DEBUG_INFO, "Bye-bye...");
    close_graphics();
    return 0;
}
