#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../timeline/timeline.h"
#include "../util/hashmap.h"
#include "../image32.h"
#include "gif.h"
#include "../util/misc.h"
#include "../debug.h"
#include "../graphics/graphics.h"
#include "../lzw/lzw.h"

/* void lzw_encode_init(lzw_encode_state_t *state, uint8_t min_code_size); - just allocate the state
 u *int8_t* lzw_encode_feed(lzw_encode_state_t *state, const uint8_t *data,
 size_t data_size, size_t *encoded_size); - returns encoded bytes
 uint8_t* lzw_encode_finish(lzw_encode_state_t *state, size_t *encoded_size); - returns encoded bytes */

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define KILLGIF {fclose(fd); if (!writeLock) {free(sourceFrame->pixels); free(sourceFrame);} return 0;}

extern struct Timeline tracks[];
extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;
extern uint32_t* pixelPreview;
extern uint32_t timelineLengthMs;

uint8_t gct_size_value(uint16_t shared_colors) {
    if (shared_colors == 0) return 0;
    uint8_t gct_size = 0;
    while (shared_colors > 2) {
        shared_colors >>= 1;
        gct_size++;
    }
    return gct_size;
}

struct frameV2* merge_frameV2(struct frameV2* dest, struct frameV2* source,
                              Rect destinfo, Rect sourceinfo, Rect* newinfo) {
    // creates new frameV2 for render only
    // calculate new bounds and allocate new pixel buffer
    int minX = MIN(sourceinfo.x, destinfo.x);
    int minY = MIN(sourceinfo.y, destinfo.y);
    int maxX = MAX(sourceinfo.x + sourceinfo.width, destinfo.x + destinfo.width);
    int maxY = MAX(sourceinfo.y + sourceinfo.height, destinfo.y + destinfo.height);
    int newWidth = maxX - minX;
    int newHeight = maxY - minY;

    uint8_t* pixels = malloc(newWidth*newHeight);
    if (!pixels) return 0;

    struct frameV2* newFrame = malloc(sizeof(struct frameV2));
    if (!newFrame) {free(pixels); return 0;}

    newFrame->delay = dest->delay;
    newFrame->pixels = pixels;

    newinfo->x = minX;
    newinfo->y = minY;
    newinfo->width = newWidth;
    newinfo->height = newHeight;

    if (dest->palette_size + source->palette_size < 256) {
        // thats easy
        for (uint16_t sourceId = 0; sourceId < source->palette_size; sourceId++) {
            newFrame->palette[sourceId] = source->palette[sourceId];
        }
        for (uint16_t destId = 0; destId < dest->palette_size; destId++) {
            newFrame->palette[source->palette_size + destId] = dest->palette[destId];
        }
        newFrame->palette_size = source->palette_size + dest->palette_size;
        newFrame->transp_idx = (source->transp_idx == 0xFF) ? dest->transp_idx : source->transp_idx;

        memset(pixels, newFrame->transp_idx, newWidth * newHeight);
        // copy source frame, then dest
        for (int x = sourceinfo.x; x<sourceinfo.x + sourceinfo.width; x++) {
            for (int y = sourceinfo.y; y<sourceinfo.y + sourceinfo.height; y++) {
                uint32_t targetX = x - minX;
                uint32_t targetY = y - minY;
                uint32_t writeTo = targetY * newWidth + targetX;
                if (writeTo >= newWidth * newHeight) continue;
                uint32_t writeFrom = (y - sourceinfo.y) * sourceinfo.width + (x - sourceinfo.x);
                if (writeFrom >= sourceinfo.width * sourceinfo.height) continue;
                pixels[writeTo] = source->pixels[writeFrom];
            }
        }
        for (int x = destinfo.x; x<destinfo.x + destinfo.width; x++) {
            for (int y = destinfo.y; y<destinfo.y + destinfo.height; y++) {
                uint32_t targetX = x - minX;
                uint32_t targetY = y - minY;
                uint32_t writeTo = targetY * newWidth + targetX;
                if (writeTo >= newWidth * newHeight) continue;
                uint32_t writeFrom = (y - destinfo.y) * destinfo.width + (x - destinfo.x);
                if (writeFrom >= destinfo.width * destinfo.height) continue;
                pixels[writeTo] = dest->pixels[writeFrom] + source->palette_size;
            }
        }
        return newFrame;
    } else {
        // Noo whyy
        // todo
        return 0;
    }

}

uint8_t export_gif(char* filepath, uint8_t export_option) {
    uint64_t time_start = current_time_ms();
    uint32_t global_palette[256] = {0};
    uint16_t shared_colors = 0;
    uint16_t bg_color_found = 0xFFFF;
    uint8_t use_gct = 0;

    uint32_t last_timestamp = 0;

    FILE* fd = fopen(filepath, "wb");
    if (!fd) return 0;
    if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}

    // todo: it would be nice here to render all of the text objects into frameV2's
    // but this is for way later and we can skip this for now since
    // we dont even have text support yet

    /*

    Note this is commented out because currently no code paths
    use the GCT and this is a useless expensive calculation

    if (export_option & EXPORT_OPTION_NOGCT) {
        struct HashMap* colors = bw_newhashmap(4096);
        for (uint8_t track = 0; track < MAX_TRACKS; track++) {
            struct TimelineObject* crtObj = tracks[track].first;
            while (crtObj) {
                switch (crtObj->metadata->type) {
                    case FILE_ANIMATION: {
                        struct imageV2* img = crtObj->metadata->imagePtr;
                        for (uint32_t frameId = 0; frameId < img->frame_count; frameId++) {
                            struct frameV2* frame = img->frames[frameId];
                            for (uint16_t color = 0; color < frame->palette_size; color++) {
                                bw_hashincrement(colors, frame->palette[color]);
                            }
                        }
                    }
                }
                crtObj = crtObj->nextObject;
            }
        }
        bw_hashsort(colors);

        for (uint32_t crtColor = 0; crtColor < colors->keys; crtColor++) {
            uint32_t color = colors->map[crtColor].key;
            if (color > 0xFFFFFF) {
                if (bg_color_found != 0xFFFF) {
                    color &= 0xFFFFFF;
                } else {
                    bg_color_found = shared_colors;
                }
            }
            if (bg_color_found == 0xFFFF && shared_colors == 255) break;
            global_palette[shared_colors] = color;
            shared_colors++;
            if (shared_colors > 255) break;
        }
        if (bg_color_found == 0xFFFF) {
            bg_color_found = shared_colors > 255 ? 255 : shared_colors;
            global_palette[shared_colors] = 0x0;
        }
        printf("Found %u reusable colors on encoding.\n", shared_colors);
        bw_hashfree(colors);
    }*/

    uint8_t gct_size_bit = gct_size_value(shared_colors);
    uint8_t lsd[7];
    lsd[0] = (fileWidthPx & 0xFF);
    lsd[1] = (fileWidthPx >> 8) & 0xFF;
    lsd[2] = (fileHeightPx & 0xFF);
    lsd[3] = (fileHeightPx >> 8) & 0xFF;
    lsd[4] = ((shared_colors > 0) << 7) | (7 << 4) | gct_size_bit;
    lsd[5] = bg_color_found & 0xFF;
    lsd[6] = 0;

    if (!fwrite(lsd, 7, 1, fd)) {fclose(fd); return 0;}

    if (shared_colors != 0) {
        for (uint16_t color = 0; color < 1 << ((gct_size_bit & 0x7) + 1); color++) {
            uint8_t this_color[3];
            this_color[0] = (global_palette[color] >> 16) & 0xFF;
            this_color[1] = (global_palette[color] >> 8) & 0xFF;
            this_color[2] = (global_palette[color]) & 0xFF;
        }
    }

    // build data of timeline
    uint32_t frames_generated = 0;

    struct TrackStatus trackstatuses[MAX_TRACKS] = {0};
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        trackstatuses[track].processingObj = tracks[track].first;
        if (trackstatuses[track].processingObj && trackstatuses[track].processingObj->timePosMs == 0) {
            trackstatuses[track].objectValid = 1;
            trackstatuses[track].dirty = 1;
        }
    }
    uint8_t pendingRender[MAX_TRACKS] = {0};
    uint8_t pendingRenderCount = 0;
    for (uint32_t timestamp = 0; timestamp<timelineLengthMs; timestamp+=10) {
        pendingRenderCount = 0;
        for (uint8_t track = 0; track < MAX_TRACKS; track++) {
            // advance object and ensure it's valid
            struct TimelineObject* crtObj = trackstatuses[track].processingObj;
            if (!crtObj) continue;
            if (timestamp == crtObj->timePosMs + crtObj->length) {
                trackstatuses[track].processingObj = crtObj->nextObject;
                trackstatuses[track].objectValid = 0;
                trackstatuses[track].frame = 0;
                crtObj = trackstatuses[track].processingObj;
                if (!crtObj) continue;
            }
            if (!trackstatuses[track].objectValid && crtObj->timePosMs == timestamp) {
                trackstatuses[track].objectValid = 1;
                trackstatuses[track].dirty = 1;
            }
            if (!trackstatuses[track].objectValid) continue;

            // advance frame inside of object
            if (crtObj->metadata->type == FILE_ANIMATION) {
                struct frameV2* crtFrame = crtObj->metadata->imagePtr->frames[trackstatuses[track].frame];
                if (crtFrame && crtFrame->delay + crtObj->timePosMs == timestamp) {
                    trackstatuses[track].frame++;
                    trackstatuses[track].dirty = 1;
                }
            }
            if (!trackstatuses[track].dirty) continue;
            trackstatuses[track].dirty = 0;
            pendingRender[pendingRenderCount] = track;
            pendingRenderCount++;
        }
        if (!pendingRenderCount) continue;
        debugf(DEBUG_VERBOSE, "%u ms: pending %u objects", timestamp, pendingRenderCount);

        // go through all the layers to prevent clipping
        uint8_t pendingRenderPtr = 0;
        uint8_t writeLock = 1;

        // WTF?
        struct imageV2* imgptr = trackstatuses[pendingRender[0]].processingObj->metadata->imagePtr;
        struct frameV2* sourceFrame = imgptr->frames[trackstatuses[pendingRender[0]].frame];
        Rect frameInfo = {0};
        frameInfo.width = imgptr->width;
        frameInfo.height = imgptr->height;
        frameInfo.x = trackstatuses[pendingRender[0]].processingObj->x;
        frameInfo.y = trackstatuses[pendingRender[0]].processingObj->y;

        for (uint8_t crtTrack = 0; crtTrack<MAX_TRACKS; crtTrack++) {
            if (trackstatuses[crtTrack].objectValid) {
                struct imageV2* imgptrdest = trackstatuses[crtTrack].processingObj->metadata->imagePtr;
                if (pendingRender[pendingRenderPtr] == crtTrack) {
                    // render onto the source for the final thing
                    if (sourceFrame != imgptrdest->frames[trackstatuses[crtTrack].frame]) {
                    pendingRenderPtr++;
                    Rect destInfo = {0};
                    destInfo.width = imgptrdest->width;
                    destInfo.height = imgptrdest->height;
                    destInfo.x = trackstatuses[crtTrack].processingObj->x;
                    destInfo.y = trackstatuses[crtTrack].processingObj->y;
                    Rect newInfo;
                    struct frameV2* newFrame = merge_frameV2(sourceFrame, imgptrdest->frames[trackstatuses[crtTrack].frame], frameInfo, destInfo, &newInfo);
                    sourceFrame = newFrame;
                    frameInfo = newInfo;
                    writeLock = 0;
                    }
                } else {
                    // clip this track out
                    
                    if (writeLock) {
                        uint8_t* pixels = malloc(frameInfo.width * frameInfo.height);
                        if (!pixels) return 0;
                        struct frameV2* newFrame = malloc(sizeof(struct frameV2));
                        if (!newFrame) {free(pixels); return 0;}
                        memcpy(newFrame, sourceFrame, sizeof(struct frameV2));
                        newFrame->pixels = pixels;
                        memcpy(pixels, sourceFrame->pixels, frameInfo.width * frameInfo.height);
                        sourceFrame = newFrame;
                    }
                    // todo: set all the non-transparent frames on the frame of this track
                    // to transparent on our current frame

                    struct frameV2* foreignFrame = imgptrdest->frames[trackstatuses[crtTrack].frame];
                    uint16_t transp_idx = foreignFrame->transp_idx;
                    uint16_t transp_idx_local = sourceFrame->transp_idx;

                    int min_x = MAX(trackstatuses[crtTrack].processingObj->x, frameInfo.x);
                    int max_x = MIN(trackstatuses[crtTrack].processingObj->x + imgptrdest->width, frameInfo.x + frameInfo.width);

                    int min_y = MAX(trackstatuses[crtTrack].processingObj->y, frameInfo.y);
                    int max_y = MIN(trackstatuses[crtTrack].processingObj->y + imgptrdest->height, frameInfo.y + frameInfo.height);

                    for (int x = min_x; x < max_x; x++) {
                        for (int y = min_y; x < max_y; y++) {
                            uint32_t localx = (x - frameInfo.x);
                            uint32_t localy = (y - frameInfo.y);

                            uint32_t foreignx = (x - trackstatuses[crtTrack].processingObj->x);
                            uint32_t foreigny = (y - trackstatuses[crtTrack].processingObj->y);

                            uint8_t pixelF = foreignFrame->pixels[foreigny * imgptrdest->width + foreignx];
                            if (pixelF == transp_idx) continue;

                            uint32_t dest_pix = localy * frameInfo.width + localx;
                            if (dest_pix > frameInfo.width * frameInfo.height) continue;

                            sourceFrame->pixels[dest_pix] = transp_idx_local;
                        }
                    }
                }
            }
        }
        // todo: write gce and lzw image here
        // also do not write parts that are out of bounds
        uint8_t gce_block[6] = {0};
        gce_block[0] = 4;
        gce_block[5] = 0;

        uint16_t delay_cs = (timestamp - last_timestamp) / 10;
        last_timestamp = timestamp;
        
        gce_block[2] = (delay_cs & 0xFF);
        gce_block[3] = (delay_cs >> 8) & 0xFF;

        gce_block[4] = sourceFrame->transp_idx;

        gce_block[1] = 1 | (0 << 2);

        if (!fwrite("\x21\xF9", 2, 1, fd)) KILLGIF
        if (!fwrite(gce_block, 6, 1, fd)) KILLGIF

        uint8_t img_desc[10];
        img_desc[0] = 0x2C;
        img_desc[1] = frameInfo.x & 0xFF;
        img_desc[2] = (frameInfo.x >> 8) & 0xFF;
        img_desc[3] = frameInfo.y & 0xFF;
        img_desc[4] = (frameInfo.y >> 8) & 0xFF;
        img_desc[5] = frameInfo.width & 0xFF;
        img_desc[6] = (frameInfo.width >> 8) & 0xFF;
        img_desc[7] = frameInfo.height & 0xFF;
        img_desc[8] = (frameInfo.height >> 8) & 0xFF;
        // TODO: temporarily write all 256 colors but simplify this in the future
        img_desc[9] = (!use_gct << 7) | 7;

        if (!fwrite(img_desc, 10, 1, fd)) KILLGIF

        uint8_t* colors = malloc(256 * 3 * 1);
        if (!colors) KILLGIF

        //uint32_t palette[256];
        //uint16_t palette_size;

        for (uint16_t idx = 0; idx<256; idx++) {
            uint32_t color = sourceFrame->palette[idx];
            colors[idx*3] = (color >> 16) & 0xFF;
            colors[idx*3+1] = (color >> 8) & 0xFF;
            colors[idx*3+2] = (color) & 0xFF;
        }

        if (!fwrite(colors, 256 * 3 * 1, 1, fd)) KILLGIF
        free(colors);

        // here we're meant to actually lzw the stuff
        // "Normally this will be the same as the number of color bits."
        lzw_encode_state_t* lzw_state = malloc(sizeof(lzw_encode_state_t));
        if (!lzw_state) KILLGIF

        uint8_t mincodesize = 8 + 1;
        if (!fwrite(&mincodesize, 1, 1, fd)) KILLGIF
        lzw_encode_init(lzw_state, mincodesize);

        // i think it is reasonable to feed the data in 255 byte chunks;
        uint32_t size_left = (frameInfo.width * frameInfo.height);
        uint8_t* ptr = sourceFrame->pixels;
        uint32_t chunk_count = (frameInfo.width * frameInfo.height) / 255 + 1;
        for (uint32_t crt_chunk = 0; crt_chunk < chunk_count; crt_chunk++) {
            uint16_t to_encode = MIN(size_left, 255);
            if (to_encode == 0) break;

            size_t lzw_size = 0;
            uint8_t* lzw_data = lzw_encode_feed(lzw_state, ptr, to_encode, &lzw_size);
            if (lzw_size > 255) KILLGIF

            uint8_t safe_size = (uint8_t) lzw_size;
            if (!fwrite(&safe_size, 1, 1, fd)) KILLGIF
            if (!fwrite(lzw_data, lzw_size, 1, fd)) KILLGIF

            ptr += to_encode;
            size_left -= to_encode;
        }

        size_t lzw_size = 0;
        uint8_t* lzw_data = lzw_encode_finish(lzw_state, &lzw_size);
        if (lzw_size > 255) KILLGIF

        uint8_t safe_size = (uint8_t) lzw_size;
        if (!fwrite(&safe_size, 1, 1, fd)) KILLGIF
        if (!fwrite(lzw_data, lzw_size, 1, fd)) KILLGIF
        safe_size = 0;
        if (!fwrite(&safe_size, 1, 1, fd)) KILLGIF

        // hooray we're done encoding this frame!!
        if (!writeLock) {free(sourceFrame->pixels); free(sourceFrame);}
        
        frames_generated++;
    }
    uint64_t time_end = current_time_ms();
    uint8_t done = 0x3B;
    if (!fwrite(&done, 1, 1, fd)) {fclose(fd); return 0;}
    fclose(fd);

    debugf(DEBUG_INFO, "Generated %u frames in %u ms.", frames_generated, time_end - time_start);

    return 1;
}
