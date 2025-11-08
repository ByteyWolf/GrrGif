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
#include "modfunc.h"

extern struct Timeline tracks[];
extern uint32_t crtTimelineMs;

extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

extern uint8_t previewPlaying;
extern uint32_t crtTimelineMs;

uint32_t* pixelPreview = NULL;
uint32_t previewHeight;
uint32_t previewWidth;
Mutex rendering;

extern struct ModuleWindow windows[3];
extern const uint32_t COLOR_GRAY;

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

void preview_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    //mutex_lock(&rendering);
    uint32_t canvas_width = width;
    uint32_t canvas_height = height;
    uint32_t offset_x = 0;
    uint32_t offset_y = 0;
    
    fit_aspect_ratio(fileWidthPx, fileHeightPx, &offset_x, &offset_y, &width, &height);
    
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
                pixelPreview[canvas_y * previewWidth + canvas_x] = 0xFFFFFFFF;
            }
        }
    }

    
    uint32_t* usePreview = pixelPreview;
    for (uint8_t track = 0; track<MAX_TRACKS-1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        
        while (crtObj) {
            //debugf(DEBUG_VERBOSE, "drawing track %u! first: %p last: %p crt: %p", track, tracks[track].first, tracks[track].last, crtObj);
            uint32_t begin = crtObj->timePosMs;
            uint32_t len = crtObj->length;
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
                
                switch (crtObj->metadata->type) {
                    case FILE_ANIMATION:
                    case FILE_STILLFRAME: {
                        uint32_t frameId = 0;
                        if (crtObj->metadata->type == FILE_ANIMATION) {
                            // maybe binary search here would be better?
                            for (frameId = 0; frameId<crtObj->metadata->imagePtr->frame_count-1; frameId++) {
                                uint32_t absoluteMs = crtObj->metadata->imagePtr->frames[frameId]->delay + begin;
                                if (absoluteMs >= crtTimelineMs) break;
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
                        draw_imageV2(crtObj->metadata->imagePtr, frameId, &dest);
                        break;
                    }
                }
            }
            crtObj = crtObj->nextObject;
        }
    }
    

    blit_rgb8888(usePreview, previewWidth, previewHeight, x + 20, y + 30);
    //mutex_unlock(&rendering);
}
