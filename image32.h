#include <stdint.h>

struct image32 {
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    struct frame32** frames;
};

struct frame32 {
    uint32_t* pixels; //width * height
    uint32_t delay;
};
