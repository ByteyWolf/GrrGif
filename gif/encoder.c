#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../timeline/timeline.h"
#include "../util/hashmap.h"

extern struct Timeline tracks[];

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
                    for (uint32_t frame = 0; frame < img->frame_count; frame++) {
                        struct frameV2* frame = img->frames[frame];
                        for (uint16_t color = 0; color < 256; color++) {
                            // todo, and also fix the max color bound pls in decoder.c thx
                        }
                    }

                }
            }
            crtObj = crtObj->nextObject;
        }
    }
    bw_hashfree(colors);
        
    FILE* fd = fopen(filepath, "wb");
    if (!fd) return 0;

    if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}
    return 0;
}
