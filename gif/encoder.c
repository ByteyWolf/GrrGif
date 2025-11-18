#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../timeline/timeline.h"

extern struct Timeline tracks[];

uint8_t export_gif(char* filepath, uint32_t max_colors) {
    // Need to find all the unique colors
    uint32_t colors = 0;
    for (uint8_t track = 0; track < MAX_TRACKS - 1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        while (crtObj) {
            
        }
    }
    
    
    FILE* fd = fopen(filepath, "wb");
    if (!fd) return 0;

    if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}
    return 0;
}
