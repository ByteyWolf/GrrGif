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

struct gce_data {
    uint16_t delay_time;
    uint8_t transparent_index;
    uint8_t disposal_method;
    uint8_t transparent_color_flag;
};