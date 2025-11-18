#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../timeline/timeline.h"

extern struct Timeline tracks[];

uint8_t export_gif(char* filepath, uint32_t max_colors) {

    /* Here's a few strategies I can use for picking colors.

    - 255 colors: I can find all colors present in the GIF
    and get up to 255 most used ones. Then, I can encode all
    of them into the Global Color Table and save a lot of
    GIF space that way.

    - Full Color: All timeline objects already have local
    palettes for each frame. So, I can just copy them
    naively into the resulting GIF. This is the simplest
    approach but the GIF ends up very large.

    - Shared Full Color: Same as above, but we can put
    colors shared between multiple frames into the GCT
    based on the amount of frames using said color(s).

    - Adaptive Full Color: Same as above, but also
    merge similar colors. Produces best file size.



    There are also some problems with GIF frame delays.
    In most GIF players these days, a delay of 0 actually
    is interpreted as a delay of 10.
    This can be worked around by setting the delay to 1,
    however it's not a nice solution if, for instance,
    two frames on two different tracks begin at the same
    time. In that case, we'd either need to indeed set
    the delay to 1 or 2, or merge the two frames together.
    Note that this causes color loss, and the user should
    probably be warned about it.

    */
    
    
    FILE* fd = fopen(filepath, "wb");
    if (!fd) return 0;

    if (!fwrite("GIF89a", 6, 1, fd)) {fclose(fd); return 0;}
    return 0;
}
