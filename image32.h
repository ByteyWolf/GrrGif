#pragma once

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

struct imageV2 {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    struct frameV2** frames;
};

struct frameV2 {
    uint32_t delay;
    uint32_t palette[256];
    uint16_t palette_size;
    uint16_t transp_idx;
    uint8_t* pixels;
};

struct copydata {
    uint32_t* pixels;
    uint32_t bufferWidth;
    uint32_t bufferHeight;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
};

void draw_imageV2(struct imageV2* img, uint32_t frameId, struct copydata* destination);
