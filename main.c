#include "graphics/graphics.h"
#include "gif/gif.h"
#include "modules.h"
#include "timeline/timeline.h"
#include "debug.h"
#include "graphics/gui_utility.h"
#include "projectformat.h"

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

const char* IDENT_STR = "GrrGif Alpha v0.1.0\n(c) Copyright 2025 Bytey Wolf. All rights reserved.\n\n";

const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_GRAY = 0x151515;

extern uint32_t wwidth;
extern uint32_t wheight;

extern uint8_t exportMode;

extern uint8_t previewPlaying;

uint8_t pendingRedraw = 1;
uint8_t eligibleToDragBorder = DRAG_BORDER_NONE;
uint8_t draggingWindowBorder = DRAG_BORDER_NONE;
uint32_t initialPos = 0;

extern struct ModuleWindow windows[3];
extern struct GUIButton* buttonBase;
extern struct GUITextBox* textBoxBase;

extern struct GUIButton* crtButtonHovering;
extern struct GUIButton* crtButtonHeld;

extern struct GUITextBox* crtTextBoxTyping;

uint8_t mouseDown = 0;
uint64_t lastRenderMs = 0;

void shrink_rect(Rect* rect, int pixels) {
    rect->x += pixels;
    rect->y += pixels;
    rect->width -= pixels * 2;
    rect->height -= pixels * 2;
}

void hoverLogic(struct GUIButton* crtButton, Event* event) {
    if (!crtButton) {debugf(DEBUG_INFO, "Button hover error"); return;}
            
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
    uint64_t crtTime = current_time_ms();
    if (crtTime - lastRenderMs < 32) {Sleep(2); return;}
    lastRenderMs = crtTime;
    //while (1) {
        timeline_heartbeat();
        if (!pendingRedraw) return; // continue;
        pendingRedraw = 0;

        windows[POSITION_BOTTOM].width = wwidth;
        windows[POSITION_TOPLEFT].height = (wheight - windows[POSITION_BOTTOM].height);
        windows[POSITION_TOPRIGHT].height = (wheight - windows[POSITION_BOTTOM].height);
        windows[POSITION_TOPRIGHT].width = (wwidth - windows[POSITION_TOPLEFT].width);
        
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
        properties_draw(0, 0, windows[POSITION_TOPLEFT].width, windows[POSITION_TOPLEFT].height);
        
        redraw_ui_elements();
    
        flush_graphics();
    //}
}

struct LoadedFile* getMetadata(char* filename) {
    struct LoadedFile* metadata = findLoadedFile(filename);
    if (!metadata) {
        struct imageV2* imgLoaded = parse(filename);
        if (!imgLoaded) { messagebox("GrrGif", "Failed to load file!", MSGBOX_ERROR); return 0;}
        
        metadata = malloc(sizeof(struct LoadedFile));
        metadata->imagePtr = imgLoaded;
        metadata->type = FILE_ANIMATION;
        metadata->refCount = 1;
    } else metadata->refCount++;
    return metadata;
}

void insertTrack(char* filename) {
    struct LoadedFile* metadata = getMetadata(filename);
    if (!metadata) return;

    struct TimelineObject* test1 = malloc(sizeof(struct TimelineObject));
    test1->x = 30;
    test1->y = 0;
    test1->width = metadata->imagePtr->width;
    test1->height = metadata->imagePtr->height;
    test1->effectsList = 0;
    test1->timePosMs = 0;
    test1->length = metadata->imagePtr->frames[metadata->imagePtr->frame_count-1]->delay;
    test1->metadata = metadata;
    test1->fileName = filename;
    test1->nextObject = 0;
    test1->beingDragged = 0;

    insertTimelineObjFree(test1);
}

int main(int argc, char *argv[]) {
    printf(IDENT_STR);

    int running = 1;
    Event* event = malloc(sizeof(Event));

    debugf(DEBUG_INFO, "Initializing graphics system...");

    if (!init_graphics(WINDOW_WIDTH, WINDOW_HEIGHT, event)) {
        fprintf(stderr, "Failed to initialize graphics\n");
        return 1;
    }

    uint8_t menuFile = create_menu();
    uint8_t menuTimeline = create_menu();
    uint8_t menuHelp = create_menu();
    
    //append_menu(menuFile, "New Project", COMMAND_FILE_NEW_PROJECT);
    append_menu(menuFile, "Open Project", COMMAND_FILE_OPEN_PROJECT);
    append_menu(menuFile, "Save Project", COMMAND_FILE_SAVE_PROJECT);
    append_menu_separator(menuFile);
    append_menu(menuFile, "Export GIF", COMMAND_FILE_EXPORT);
    append_menu_separator(menuFile);
    append_menu(menuFile, "Exit", COMMAND_FILE_EXIT);

    append_menu(menuTimeline, "GIF", COMMAND_ACTION_ADDTRACK);
    append_menu(menuTimeline, "Shape", COMMAND_ACTION_ADDSHAPE);
    append_menu(menuTimeline, "Text", COMMAND_ACTION_ADDTEXT);

    append_menu(menuHelp, "About GrrGif", COMMAND_HELP_ABOUT);

    finalize_menu(menuFile, "File");
    finalize_menu(menuTimeline, "Insert");
    finalize_menu(menuHelp, "Help");

    //scheduleRendering();
    set_window_title("GrrGif");

    messagebox("GrrGif", "GrrGif is work-in-progress software.\n\nYou **CANNOT** export GIFs yet.\n\nI am not responsible for any time wasted editing GIFs. Come back later and hopefully it'll be added!", MSGBOX_WARNING);
    
    while (running) {

        while (poll_event()) {
            if (event->type == EVENT_QUIT) {running = 0; break;}
            
            switch (event->type) {
                case EVENT_RESIZE:
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
                            if (eligibleToDragBorder != DRAG_BORDER_NONE) set_cursor(CURSOR_NORMAL);
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
                    mouseDown = 1;
                    crtTextBoxTyping = 0;
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

                    // select text box to click
                    struct GUITextBox* crtBox = textBoxBase;
                    while (crtBox) {
                        uint32_t left = getWindowX(crtBox->weldToWindow) + crtBox->localX;
                        uint32_t top = getWindowY(crtBox->weldToWindow) + crtBox->localY;
                        if (crtBox->state != TEXTBOX_STATE_HIDDEN &&
                            event->x > left &&
                            event->y > top &&
                            event->x < (left + crtBox->width) &&
                            event->y < (top + crtBox->height)) {

                            crtTextBoxTyping = crtBox;
                            break;
                        }
                        crtBox = crtBox->nextTextBox;
                    }
                    
                    break;
                case EVENT_MOUSEBUTTONUP:
                    mouseDown = 0;
                    draggingWindowBorder = DRAG_BORDER_NONE;
                    //setButtonState(crtButtonHeld, BUTTON_STATE_NORMAL);
                    if (crtButtonHeld)  hoverLogic(crtButtonHeld, event);
                    
                    break;
                case EVENT_MOUSESCROLL:
                    if (event->y > windows[POSITION_TOPLEFT].height) {
                        timeline_scroll(event->scrollDelta);
                    }
                    break;
                case EVENT_COMMAND:
                    switch (event->command) {
                        case COMMAND_FILE_EXIT:
                            running = 0;
                            break;

                        case COMMAND_FILE_SAVE_PROJECT: {
                            char* filePath = choose_save_file(FILETYPE_GRRPROJ);
                            if (!filePath) break;
                            if (!saveProject(filePath)) messagebox("GrrGif", "Failed to save project.", MSGBOX_ERROR);
                            free(filePath);
                            break;
                        }

                        case COMMAND_FILE_OPEN_PROJECT: {
                            //todo: clean current project
                            char* filePath = choose_file(FILETYPE_GRRPROJ);
                            if (!filePath) break;
                            if (!loadProject(filePath)) messagebox("GrrGif", "Failed to load project.", MSGBOX_ERROR);
                            pendingRedraw = 1;
                            free(filePath);
                            break;
                        }

                        case COMMAND_FILE_EXPORT: {
                            char* filePath = choose_file(FILETYPE_GIF);
                            export_gif(filePath, exportMode);
                            messagebox("GrrGif", "This is not yet implemented!", MSGBOX_WARNING);
                            break;
                        }
                            
                        case COMMAND_ACTION_ADDTRACK: {
                            char* filePath = choose_file(FILETYPE_GIF);
                            if (!filePath) break;
                            insertTrack(filePath);
                            pendingRedraw = 1;
                            break;
                        }
                        case COMMAND_HELP_ABOUT: {
                            char* strbuf = malloc(2048);
                            if (!strbuf) {messagebox("GrrGif", "Insufficient memory", MSGBOX_ERROR); break;}

                            HANDLE hHeap = GetProcessHeap();
                            if (!hHeap) {messagebox("GrrGif", "Error getting heap data", MSGBOX_ERROR); break;}

                            PROCESS_HEAP_ENTRY entry;
                            SIZE_T totalUsed = 0;

                            entry.lpData = NULL;

                            while (HeapWalk(hHeap, &entry)) {
                                if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
                                    totalUsed += entry.cbData;
                                }
                            }

                            if (GetLastError() != ERROR_NO_MORE_ITEMS) {messagebox("GrrGif", "Error getting heap data", MSGBOX_ERROR); break;}
                            
                            snprintf(strbuf, 2047, "%sMemory used by GrrGif: %u bytes", IDENT_STR, totalUsed);
                            messagebox("About GrrGif", strbuf, MSGBOX_INFO);
                            break;
                        }
                    }
                    break;
            }
            preview_handle_event(event);
            timeline_handle_event(event);
            renderUI();
        }
        Sleep(1);

        //clear_graphics(0x000000);
        //if (previewPlaying) preview_draw(wwidth - windows[POSITION_TOPRIGHT].width, 0, windows[POSITION_TOPRIGHT].width, windows[POSITION_TOPRIGHT].height);
        renderUI();
        
    }

    debugf(DEBUG_INFO, "Bye-bye...");
    close_graphics();
    return 0;
}
