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
    memset(new, 0, previewWidth * previewHeight * sizeof(uint32_t));
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

void preview_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    mutex_lock(&rendering);
    if (!pixelPreview) alloc_preview();
    uint32_t* usePreview = pixelPreview;
    struct TimelineObject* crtObj = timeline;
    while (crtObj) {
        uint32_t begin = crtObj->timePosMs;
        uint32_t len = crtObj->length;
        printf("obj%p\n", crtObj);
        if (begin <= crtTimelineMs && begin + len >= crtTimelineMs) {
            // eligible for rendering
            // step 1: draw preview
            // step 2: TODO: draw effects: to be added later
            // note for self: we need to add padding to the preview field when i implement effects, for stuff like borders
            if (pixelPreview != usePreview) {
                // preview area has changed, abort drawing
                crtObj = NULL;
                alloc_preview();
                mutex_unlock(&rendering);
                return;
            }

            float scaleX = (float)previewWidth / fileWidthPx;
            float scaleY = (float)previewHeight / fileHeightPx;

            printf("scalings: %f %f\n", scaleX, scaleY);

            // Transform the inner box
            uint32_t scaled_x = (uint32_t)(crtObj->x * scaleX);
            uint32_t scaled_y = (uint32_t)(crtObj->y * scaleY);
            uint32_t scaled_width = (uint32_t)(crtObj->width * scaleX);
            uint32_t scaled_height = (uint32_t)(crtObj->height * scaleY);

            // TODO: currently this only draws frame 0, implement for changing frames
            switch (crtObj->metadata->type) {
                case FILE_STILLFRAME:
                case FILE_ANIMATION:
                    struct copydata src = {crtObj->metadata->imagePtr->frames[0]->pixels, crtObj->metadata->imagePtr->width, crtObj->metadata->imagePtr->height, crtObj->metadata->imagePtr->width, crtObj->metadata->imagePtr->height, 0, 0};
                    struct copydata dest = {usePreview, previewWidth, previewHeight, scaled_width, scaled_height, scaled_x, scaled_y};
                    printf("Copy from %p %ux%u %u;%u to %p %ux%u %u;%u\n", src.pixels, src.width, src.height, src.x, src.y, dest.pixels, dest.width, dest.height, dest.x, dest.y);
                    nearest_neighbor(&src, &dest);
                    break;
            }
        }
        crtObj = crtObj->nextObject;
    }
    blit_rgb8888(usePreview, previewWidth, previewHeight, x + 20, y + 30);
    mutex_unlock(&rendering);
}