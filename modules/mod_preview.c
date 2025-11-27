#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../timeline/timeline.h"
#include "../graphics/graphics.h"
#include "../lock.h"
#include "../modules.h"
#include "../image32.h"
#include "../debug.h"

extern struct Timeline tracks[];
extern uint32_t crtTimelineMs;
extern uint32_t timelineLengthMs;

extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

extern uint8_t previewPlaying;

uint32_t* pixelPreview = NULL;
uint32_t previewHeight;
uint32_t previewWidth;
Mutex rendering;

extern struct ModuleWindow windows[3];
extern const uint32_t COLOR_GRAY;

struct TimelineObject* selectedObj = 0;
struct TimelineObject* objOnThisFrame[64];
uint8_t objOnThisFrameCount = 0;
extern uint8_t previewPlaying;

uint32_t canvas_x = 0;
uint32_t canvas_y = 0;
uint32_t canvas_width = 0;
uint32_t canvas_height = 0;

uint32_t lastMouseDragX = 0;
uint32_t lastMouseDragY = 0;

extern uint8_t mouseDown;

uint8_t refreshFrameCache = 0;

int alloc_preview() {
    if (pixelPreview) free(pixelPreview);
    previewWidth = windows[1].width - 40;
    previewHeight = windows[1].height - 40;
    uint32_t* new = malloc(previewWidth * previewHeight * sizeof(uint32_t));
    if (!new) return 1;
    memset(new, COLOR_GRAY, previewWidth * previewHeight * sizeof(uint32_t));
    pixelPreview = new;
    return 0;
}

void fit_aspect_ratio(uint32_t src_width, uint32_t src_height,
                      uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height) {
    float src_aspect = (float)src_width / (float)src_height;
    float dst_aspect = (float)(*width) / (float)(*height);
    
    if (dst_aspect > src_aspect) {
        uint32_t new_width = (uint32_t)((*height) * src_aspect);
        *x += (*width - new_width) / 2;
        *width = new_width;
    } else {
        uint32_t new_height = (uint32_t)((*width) / src_aspect);
        *y += (*height - new_height) / 2;
        *height = new_height;
    }
}

void draw_canvas_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) {
    if (!pixelPreview) return;
    for (int crtx = x; crtx<x+width; crtx++) {
        for (int crty = y; crty<y+height; crty++) {
            if ((crty*previewWidth+crtx)>canvas_width*canvas_height-1) break;
            if (color > 0xFFFFFF) {
                uint16_t alpha = (color >> 24) & 0xFF;
                uint16_t rD = (color >> 16) & 0xFF;
                uint16_t gD = (color >> 8) & 0xFF;
                uint16_t bD = color & 0xFF;

                uint32_t source = pixelPreview[crty*previewWidth+crtx];
                uint8_t rS = (source >> 16) & 0xFF;
                uint8_t gS = (source >> 8) & 0xFF;
                uint8_t bS = source & 0xFF;

                rD = (alpha * rD + (255 - alpha) * rS) / 255;
                gD = (alpha * gD + (255 - alpha) * gS) / 255;
                bD = (alpha * bD + (255 - alpha) * bS) / 255;

                color = (rD << 16) | (gD << 8) | bD;
            }
            pixelPreview[crty*previewWidth+crtx]=color;
        }
    }
}

void preview_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (width < 50 || width > 0xFFFF || height < 50 || height > 0xFFFFF) return;
    //mutex_lock(&rendering);
    uint32_t offset_x = 0;
    uint32_t offset_y = 0;
    
    fit_aspect_ratio(fileWidthPx, fileHeightPx, &offset_x, &offset_y, &width, &height);

    canvas_width = width;
    canvas_height = height;
    
    if (canvas_width != previewWidth || canvas_height != previewHeight) {
        free(pixelPreview); 
        pixelPreview = NULL; 
        previewWidth = canvas_width; 
        previewHeight = canvas_height;
    }
    
    if (!pixelPreview) alloc_preview();
    for (uint32_t py = 0; py < height; py++) {
        for (uint32_t px = 0; px < width; px++) {
            uint32_t canvas_x = offset_x + px;
            uint32_t canvas_y = offset_y + py;
            if (canvas_x < previewWidth && canvas_y < previewHeight) {
                pixelPreview[canvas_y * previewWidth + canvas_x] = (canvas_x+(canvas_y*3))%2 ? 0xFFFFFF : 0x999999;
            }
        }
    }

    
    uint32_t* usePreview = pixelPreview;
    if (refreshFrameCache) objOnThisFrameCount = 0;

    uint32_t furthestMs = 0;
    
    for (uint8_t track = 0; track<MAX_TRACKS-1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        
        while (crtObj) {
            //debugf(DEBUG_VERBOSE, "drawing track %u! first: %p last: %p crt: %p", track, tracks[track].first, tracks[track].last, crtObj);
            uint32_t begin = crtObj->timePosMs;
            uint32_t len = crtObj->length;
            if (begin+len > furthestMs) furthestMs = begin+len;
            if (begin <= crtTimelineMs && begin + len >= crtTimelineMs) {
                if (pixelPreview != usePreview) {
                    crtObj = NULL;
                    alloc_preview();
                    mutex_unlock(&rendering);
                    return;
                }
    
                float scaleX = (float)width / fileWidthPx;
                float scaleY = (float)height / fileHeightPx;
                
                uint32_t scaled_x = offset_x + (uint32_t)(crtObj->x * scaleX);
                uint32_t scaled_y = offset_y + (uint32_t)(crtObj->y * scaleY);
                uint32_t scaled_width = (uint32_t)(crtObj->width * scaleX);
                uint32_t scaled_height = (uint32_t)(crtObj->height * scaleY);
                
                if (refreshFrameCache && objOnThisFrameCount < 63) {
                    objOnThisFrame[objOnThisFrameCount] = crtObj;
                    objOnThisFrameCount++;
                }
                
                switch (crtObj->metadata->type) {
                    case FILE_ANIMATION:
                    case FILE_STILLFRAME: {
                        uint32_t frameId = 0;
                        if (crtObj->metadata->type == FILE_ANIMATION) {
                            // todo: change the line below to add non-loop
                            uint32_t imgDuration = crtObj->metadata->imagePtr->frames[crtObj->metadata->imagePtr->frame_count-1]->delay;
                            uint32_t relativeCrtMs = (crtTimelineMs - begin) % imgDuration;
                            struct imageV2* imagePtr = crtObj->metadata->imagePtr;
                            // maybe binary search here would be better?
                            for (frameId = 0; frameId<imagePtr->frame_count-1; frameId++) {
                                uint32_t absoluteMs = imagePtr->frames[frameId]->delay;
                                if (absoluteMs >= relativeCrtMs) break;
                            }
                        }
                        
    
                        struct copydata dest;
                        dest.pixels = usePreview;
                        dest.bufferWidth = previewWidth;
                        dest.bufferHeight = previewHeight;
                        dest.width = scaled_width;
                        dest.height = scaled_height;
                        dest.x = scaled_x;
                        dest.y = scaled_y;
                        if (crtObj == selectedObj) {
                            draw_canvas_rect(scaled_x - 1, scaled_y - 1, scaled_width + 4, scaled_height + 4, 0x99000000);
                        }
                        draw_imageV2(crtObj->metadata->imagePtr, frameId, &dest);
                        break;
                    }
                }
            }
            crtObj = crtObj->nextObject;
        }
    }
    timelineLengthMs = furthestMs;
    //printf("waow %p %ux%u %u %u\n", usePreview, previewWidth, previewHeight, x+20, y+30);
    canvas_x = x+20+offset_x;
    canvas_y = y+30+offset_y;
    blit_rgb8888(usePreview, previewWidth, previewHeight, x + 20, y + 30);
    refreshFrameCache = 0;
    //Sleep(1000);
    //mutex_unlock(&rendering);
}

int convertMouseXtoCanvasX(int mouseX) {
    float scaleratio = (float)fileWidthPx / (float)canvas_width;
    return scaleratio * mouseX;
}

int convertMouseYtoCanvasY(int mouseY) {
    float scaleratio = (float)fileHeightPx / (float)canvas_height;
    return scaleratio * mouseY;
}

struct TimelineObject* getHoveringObj(Event* event) {
    if (event->x < canvas_x ||
        event->x > canvas_x+canvas_width ||
        event->y < canvas_y ||
        event->y > canvas_y + canvas_height) return 0;
    uint32_t mouseXCanvas = convertMouseXtoCanvasX(event->x - canvas_x);
    uint32_t mouseYCanvas = convertMouseYtoCanvasY(event->y - canvas_y);
    lastMouseDragX = mouseXCanvas;
    lastMouseDragY = mouseYCanvas;

    struct TimelineObject* bestObj = 0;
    uint8_t bestTrack = 0;
    
    for (uint8_t objId = 0; objId<objOnThisFrameCount; objId++) {
        struct TimelineObject* obj = objOnThisFrame[objId];
        if (mouseXCanvas > obj->x &&
            mouseXCanvas < obj->x + obj->width &&
            mouseYCanvas > obj->y &&
            mouseYCanvas < obj->y + obj->height) {
            
            if (obj->track >= bestTrack) {
                bestObj = obj;
                bestTrack = obj->track;
            }
        }
    }
    return bestObj;
}

void preview_handle_event(Event* event) {
    if (!event) return;
    if (event->x <= windows[POSITION_TOPLEFT].width || event->y >= windows[POSITION_TOPLEFT].height) return;
    switch (event->type) {
        case EVENT_MOUSEBUTTONDOWN: {
            selectedObj = 0;
            struct TimelineObject* hoveringObj = getHoveringObj(event);
            selectedObj = hoveringObj; // no null check is intentional!
            //mouseDown = 1;
            break;
        }
        case EVENT_MOUSEMOVE:
            if (selectedObj && mouseDown) {
                set_cursor(CURSOR_MOVE);
                int mouseX = convertMouseXtoCanvasX(event->x - canvas_x);
                int mouseY = convertMouseYtoCanvasY(event->y - canvas_y);
                int deltaX = mouseX - lastMouseDragX;
                int deltaY = mouseY - lastMouseDragY;
                selectedObj->x += deltaX;
                selectedObj->y += deltaY;
                lastMouseDragX = mouseX;
                lastMouseDragY = mouseY;
                break;
            }
            if (getHoveringObj(event)) { set_cursor(CURSOR_MOVE); } else {set_cursor(CURSOR_NORMAL);}
            break;
            
        case EVENT_MOUSEBUTTONUP:
            //mouseDown = 0;
            //selectedObj = 0;
            //didDrag = 0;
            break;
    }
}
