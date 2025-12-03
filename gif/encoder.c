#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../timeline/timeline.h"
#include "../util/hashmap.h"
#include "../image32.h"
#include "gif.h"
#include "../util/misc.h"

extern struct Timeline tracks[];
extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

extern uint32_t* pixelPreview;


void init_cursors(struct TimelineObject* cursors[]) {
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        cursors[track] = tracks[track].first;
        if (cursors[track]) cursors[track]->fakeTimePosMs = 0;
    }
}

struct TimelineObject* get_next_object(struct TimelineObject* cursors[], uint8_t advance) {
    struct TimelineObject* earliest_object = 0;
    uint32_t earliest_appearance = 0xFFFFFFFF;
    
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        struct TimelineObject* objHere = cursors[track];
        if (!objHere) continue;
        // reuse fakeTimePosMs for frames
        uint32_t timePosMs = objHere->timePosMs;
        if (objHere->metadata->type == FILE_ANIMATION) {
            timePosMs += objHere->metadata->imagePtr->frames[objHere->fakeTimePosMs]->delay;
        }
        
        if (timePosMs < earliest_appearance) {
            earliest_object = objHere;
            earliest_appearance = objHere->timePosMs;
        }
    }
    if (advance && earliest_object) {
        if (earliest_object->metadata->type == FILE_ANIMATION) {
            uint32_t frames = earliest_object->metadata->imagePtr->frame_count;
            if (earliest_object->fakeTimePosMs >= frames - 1) {
                cursors[earliest_object->track] = earliest_object->nextObject;
                if (earliest_object->nextObject) earliest_object->nextObject->fakeTimePosMs = 0;
            } else {
                earliest_object->fakeTimePosMs++;
            }
            
        } else
            cursors[earliest_object->track] = earliest_object->nextObject;
    }
    
    return earliest_object;
}

uint8_t gct_size_value(uint16_t shared_colors) {
    if (shared_colors == 0) return 0;
    uint8_t gct_size = 0;
    while (shared_colors > 2) {
        shared_colors >>= 1;
        gct_size++;
    }
    return gct_size;
}

// todo: gifs layered atop each other that arent merged must clip
uint8_t export_gif(char* filepath, uint8_t export_option) {    
    // get most popular colors in gif
    uint32_t global_palette[256] = {0};
    uint16_t shared_colors = 0;
    uint16_t bg_color_found = 0xFFFF;
    
    if (export_option & EXPORT_OPTION_NOGCT) {
        struct HashMap* colors = bw_newhashmap(4096); // about 32 kb
        for (uint8_t track = 0; track < MAX_TRACKS; track++) {
            struct TimelineObject* crtObj = tracks[track].first;
            while (crtObj) {
                // loop through metadata here (too lazy to add rn)
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
    }
    
    // file
    //FILE* fd = fopen(filepath, "wb");
    //if (!fd) return 0;

    //if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}

    // lsd
    uint8_t gct_size_bit = gct_size_value(shared_colors);
    uint8_t lsd[7];
    lsd[0] = (fileWidthPx & 0xFF);
    lsd[1] = (fileWidthPx >> 8) & 0xFF;
    lsd[2] = (fileHeightPx & 0xFF);
    lsd[3] = (fileHeightPx >> 8) & 0xFF;
    lsd[4] = ((shared_colors > 0) << 7) | (7 << 4) | gct_size_bit; 
    lsd[5] = bg_color_found & 0xFF;
    lsd[6] = 0;
    //if (!fwrite(lsd, 7, 1, fd)) {fclose(fd); return 0;}

    // gct
    if (shared_colors != 0) {
        for (uint16_t color = 0; color < 1 << ((gct_size_bit & 0x7) + 1); color++) {
            uint8_t this_color[3];
            this_color[0] = (global_palette[color] >> 16) & 0xFF;
            this_color[1] = (global_palette[color] >> 8) & 0xFF;
            this_color[2] = (global_palette[color]) & 0xFF;
            //if (!fwrite(this_color, 3, 1, fd)) {fclose(fd); return 0;}
        }
    }

    // start building gce's and image data here
    // first we need to assess all the frames there are
    struct TimelineObject* cursors[MAX_TRACKS] = {0};
    init_cursors(cursors);

    // overlapping gifs need to be merged on render
    struct TimelineObject* render_buffer[256] = {0};
    uint8_t gifs_on_buffer = 0;

    while (1) {
        gifs_on_buffer = 0;
        while(1) {
            struct TimelineObject* crtObj = get_next_object(cursors, 1);
            if (!crtObj) break;
            render_buffer[gifs_on_buffer] = crtObj;
            gifs_on_buffer++;
            struct TimelineObject* peek = get_next_object(cursors, 0);
            if (!peek) break;
            uint32_t peekMs = peek->timePosMs;
            if (peek->metadata->type == FILE_ANIMATION)
                peekMs += peek->metadata->imagePtr->frames[peek->fakeTimePosMs]->delay;
            uint32_t crtMs = crtObj->timePosMs;
            if (crtObj->metadata->type == FILE_ANIMATION)
                crtMs += crtObj->metadata->imagePtr->frames[crtObj->fakeTimePosMs]->delay;
            printf("crtms %u peekms %u\n", crtMs, peekMs);
            if (crtMs != peekMs) break;
        }
        if (!gifs_on_buffer) break;

        // encode
        printf("Would encode %u GIFs at time pos %u.\n", gifs_on_buffer, render_buffer[0]->timePosMs);
    }
    
   
    return 0;
}
