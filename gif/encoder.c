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

uint8_t gct_size_value(uint16_t shared_colors) {
    if (shared_colors == 0) return 0;
    uint8_t gct_size = 0;
    while (shared_colors > 2) {
        shared_colors >>= 1;
        gct_size++;
    }
    return gct_size;
}

// explanation of STRATEGY
// 0b00 - Full color, do not merge overlapping frames
// 0b01 - Full color, merge overlapping frames
// 0b10 - 256 colors, do not merge overlapping frames
// 0b11 - 256 colors, merge overlapping frames
uint8_t export_gif(char* filepath, uint8_t strategy) {
    // get most popular colors in gif
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
    uint32_t global_palette[256] = {0};
    uint16_t shared_colors = 0;
    uint16_t bg_color_found = 0xFFFF;
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
    
    // file
    FILE* fd = fopen(filepath, "wb");
    if (!fd) return 0;

    if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}

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
    if (!fwrite(lsd, 7, 1, fd)) {fclose(fd); return 0;}

    // gct
    if (shared_colors != 0) {
        for (uint16_t color = 0; color < 1 << ((gct_size_bit & 0x7) + 1); color++) {
            uint8_t this_color[3];
            this_color[0] = (global_palette[color] >> 16) & 0xFF;
            this_color[1] = (global_palette[color] >> 8) & 0xFF;
            this_color[2] = (global_palette[color]) & 0xFF;
            if (!fwrite(this_color, 3, 1, fd)) {fclose(fd); return 0;}
        }
    }

    // start building gce's and image data here
    // first we need to assess all the frames there are
    struct SimplifiedList* gifBase = malloc(sizeof(struct SimplifiedList));
    if (!gifBase) {fclose(fd); return 0;}

    // todo
    
   
    return 0;
}
