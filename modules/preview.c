#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../timeline/timeline.h"
#include "../graphics/graphics.h"
#include "../lock.h"
#include "../modules.h"
#include "../image32.h"
#include "modfunc.h"

extern struct TimelineObject** sequentialAccess;
extern struct TimelineObject* timeline;
extern uint32_t crtTimelineMs;

extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

uint32_t* pixelPreview = NULL;
uint32_t previewHeight;
uint32_t previewWidth;
Mutex rendering;

extern struct ModuleWindow windows[3];
extern const uint32_t COLOR_GRAY;

struct copydata {
    uint32_t* pixels;
    uint32_t bufferWidth;
    uint32_t bufferHeight;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
};

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


void nearest_neighbor(struct copydata* src, struct copydata* dst) {
    if (!src || !dst || !src->pixels || !dst->pixels) return;
    
    float xScale = (float)src->width / dst->width;
    float yScale = (float)src->height / dst->height;
    
    for (uint32_t dy = 0; dy < dst->height; dy++) {
        uint32_t srcY = (uint32_t)(dy * yScale);
        if (srcY >= src->height) srcY = src->height - 1;
        
        for (uint32_t dx = 0; dx < dst->width; dx++) {
            uint32_t srcX = (uint32_t)(dx * xScale);
            if (srcX >= src->width) srcX = src->width - 1;
            
            uint32_t pixel = src->pixels[srcY * src->bufferWidth + srcX];
            uint8_t alpha = pixel >> 24;
            if (alpha < 255) continue;
            
            uint32_t dstX = dst->x + dx;
            uint32_t dstY = dst->y + dy;
            
            if (dstX >= dst->bufferWidth || dstY >= dst->bufferHeight) continue;
            
            dst->pixels[dstY * dst->bufferWidth + dstX] = pixel;
        }
    }
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
    struct TimelineObject* crtObj = timeline;
    
    while (crtObj) {
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
                case FILE_STILLFRAME:
                case FILE_ANIMATION: {
                    struct copydata src;
                    src.pixels=crtObj->metadata->imagePtr->frames[0]->pixels;
                    src.bufferWidth=crtObj->metadata->imagePtr->width;
                    src.bufferHeight=crtObj->metadata->imagePtr->height;
                    src.width=crtObj->metadata->imagePtr->width;
                    src.height=crtObj->metadata->imagePtr->height;
                    src.x=0;
                    src.y=0;

                    struct copydata dest;
                    dest.pixels = usePreview;
                    dest.bufferWidth = previewWidth;
                    dest.bufferHeight = previewHeight;
                    dest.width = scaled_width;
                    dest.height = scaled_height;
                    dest.x = scaled_x;
                    dest.y = scaled_y;
                    nearest_neighbor(&src, &dest);
                    break;
                }
            }
        }
        crtObj = crtObj->nextObject;
    }
    

    blit_rgb8888(usePreview, previewWidth, previewHeight, x + 20, y + 30);
    //mutex_unlock(&rendering);
}