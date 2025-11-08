#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gif.h"
#include "../lzw/lzw.h"
#include "../image32.h"
#include "../debug.h"

static uint32_t g_global_palette[256];
static uint32_t g_local_palette[256];
static uint32_t g_width;
static uint32_t g_height;
static uint32_t g_img_left;
static uint32_t g_img_top;
static uint32_t g_local_width;
static uint32_t g_local_height;
static uint8_t g_interlace_flag;
static uint8_t g_bg_index;
static uint8_t g_current_disposal;
static int g_transp_idx;
static struct gce_data g_current_gce;
static uint32_t g_crt_delay;
static uint32_t g_crt_frame;
static struct imageV2* g_img;
static FILE* g_fd;

size_t compat_read(FILE* fd, void *buf, size_t count) {
    return fread(buf, 1, count, (FILE *)fd);
}

uint32_t* process_palette(FILE* fd, int num_entries, uint32_t* palette)
{
    int bytesRead;
    size_t gct_size;
    uint8_t *gct;
    int i;
    gct_size = 3 * num_entries;
    gct = (uint8_t *)malloc(gct_size);
    if (!gct) RFAILEDP
        bytesRead = compat_read(fd, gct, gct_size);
    if (bytesRead < (int)gct_size) {
        free(gct);
        RFAILEDP
    }
    for (i = 0; i < num_entries && i < 256; i++) {
        palette[i] = (0xFFu << 24) | ((uint32_t)gct[3*i] << 16) | ((uint32_t)gct[3*i + 1] << 8) | (uint32_t)gct[3*i + 2];
    }
    free(gct);
    return palette;
}

uint32_t calc_row_interlaced_uint(uint32_t row_index, uint32_t height) {
    uint32_t row;
    uint32_t pass1_end;
    uint32_t pass2_end;
    uint32_t pass3_end;
    pass1_end = (height + 7)/8;
    pass2_end = (height + 3)/4;
    pass3_end = (height + 1)/2;
    if (row_index < pass1_end)
        row = row_index * 8;
    else if (row_index < pass2_end)
        row = (row_index - pass1_end) * 8 + 4;
    else if (row_index < pass3_end)
        row = (row_index - pass2_end) * 4 + 2;
    else
        row = (row_index - pass3_end) * 2 + 1;
    return row;
}

int append_frame(struct imageV2* img, struct frameV2* new_frame) {
    struct frameV2** temp;
    if (!img || !new_frame) return -1;
    temp = realloc(img->frames, (img->frame_count + 1) * sizeof(struct frameV2*));
    if (!temp) return -1;
    img->frames = temp;
    img->frames[img->frame_count] = new_frame;
    img->frame_count++;
    return 0;
}

void seek_through_blocks(FILE* fd) {
    uint8_t block_size;
    size_t bytesRead;
    while (1) {
        bytesRead = compat_read(fd, &block_size, 1);
        if (bytesRead < 1) RFAILED
            if (block_size == 0) break;
            fseek(fd, block_size, SEEK_CUR);
    }
}

static int process_pixel(uint8_t* frame, uint32_t pixel_index, uint8_t color_index) {
    uint32_t pixeloff;
    uint32_t inner_row;
    uint32_t inner_col;
    uint32_t outer_row;
    uint32_t outer_col;
    uint8_t *prev_frame;
    inner_row = pixel_index / g_local_width;
    inner_col = pixel_index % g_local_width;
    outer_row = g_img_top + inner_row;
    outer_col = g_img_left + inner_col;
    if (g_interlace_flag) {
        outer_row = g_img_top + calc_row_interlaced_uint(inner_row, g_local_height);
    }
    pixeloff = outer_row * g_width + outer_col;
    if (pixeloff >= g_width * g_height) return -1;
    switch (g_current_disposal) {
        case DISPOSE_NONE:
        case DISPOSE_UNSPECIFIED:
            if (color_index != g_transp_idx) frame[pixeloff] = color_index;
            break;
        case DISPOSE_BACKGROUND:
            if (color_index != g_transp_idx) frame[pixeloff] = color_index;
            else frame[pixeloff] = g_bg_index;
            break;
        case DISPOSE_PREVIOUS:
            if (g_crt_frame == 0) return -1;
            prev_frame = g_img->frames[g_crt_frame - 1]->pixels;
        if (color_index != g_transp_idx) frame[pixeloff] = color_index;
        else frame[pixeloff] = prev_frame[pixeloff];
        break;
    }
    return 0;
}

static int process_lzw_block(lzw_state_t* decoder, uint8_t* frame, uint32_t* pixel_index) {
    uint8_t sub_block_size;
    uint8_t* sub_block_data;
    size_t outSize;
    const unsigned char* chunk;
    int px;
    size_t bytesRead;
    bytesRead = compat_read(g_fd, &sub_block_size, 1);
    if (bytesRead < 1) return -1;
    if (sub_block_size == 0) return 0;
    sub_block_data = malloc(sub_block_size);
    if (!sub_block_data) return -1;
    bytesRead = compat_read(g_fd, sub_block_data, sub_block_size);
    if (bytesRead < sub_block_size) {
        free(sub_block_data);
        return -1;
    }
    chunk = lzw_feed(decoder, sub_block_data, sub_block_size, &outSize);
    for (px = 0; px < (int)outSize; px++) {
        if (process_pixel(frame, *pixel_index, chunk[px]) != 0) {
            free(sub_block_data);
            return -1;
        }
        (*pixel_index)++;
    }
    free(sub_block_data);
    return 1;
}

static int decode_image_data(uint8_t* frame) {
    lzw_state_t* decoder = malloc(sizeof(lzw_state_t));
    uint8_t lzw_min_code_size;
    uint32_t pixel_index = 0;
    size_t bytesRead;
    int result;
    bytesRead = compat_read(g_fd, &lzw_min_code_size, 1);
    if (bytesRead < 1) RFAILEDP
        lzw_init(decoder, lzw_min_code_size);
    while (1) {
        result = process_lzw_block(decoder, frame, &pixel_index);
        if (result < 0) RFAILEDP
            if (result == 0) break;
    }
    free(decoder);
    return 0;
}

static int read_image_descriptor(void) {
    uint8_t img_desc[9];
    uint8_t img_packed;
    uint8_t lct_flag;
    int lct_entries;
    size_t bytesRead;
    bytesRead = compat_read(g_fd, img_desc, sizeof(img_desc));
    if (bytesRead < 9) RFAILEDP
        g_img_left = img_desc[0] | (img_desc[1] << 8);
    g_img_top = img_desc[2] | (img_desc[3] << 8);
    g_local_width = img_desc[4] | (img_desc[5] << 8);
    g_local_height = img_desc[6] | (img_desc[7] << 8);
    img_packed = img_desc[8];
    lct_flag = img_packed >> 7;
    g_interlace_flag = (img_packed >> 6) & 1;
    lct_entries = 1 << ((img_packed & 0x7) + 1);
    if (lct_flag) {
        process_palette(g_fd, lct_entries, g_local_palette);
    } else {
        memcpy(g_local_palette, g_global_palette, 256 * sizeof(uint32_t));
    }
    //if (g_current_gce.transparent_color_flag) {
        //uint32_t idx = g_current_gce.transparent_index;
        //if (idx < 256) g_local_palette[idx] = 0x00000000;
    //}
    //if (g_bg_index < 256) g_local_palette[g_bg_index] = 0x00000000;
    return 0;
}

static int handle_image_descriptor(void) {
    struct frameV2 *current_frame;
    uint8_t* frame;
    if (read_image_descriptor() != 0) RFAILEDP
        current_frame = malloc(sizeof(struct frameV2));
    frame = malloc(g_width * g_height * sizeof(uint8_t));
    if (!frame || !current_frame) {
        free(frame);
        free(current_frame);
        RFAILEDP
    }
    memset(frame, 0, g_width * g_height * sizeof(uint8_t));
    current_frame->pixels = frame;
    current_frame->delay = g_crt_delay;
    memcpy(current_frame->palette, g_local_palette, sizeof(current_frame->palette));
    g_crt_delay += g_current_gce.delay_time * 10;
    g_transp_idx = g_current_gce.transparent_color_flag ? g_current_gce.transparent_index : -1;
    if (decode_image_data(frame) != 0) {
        free(frame);
        free(current_frame);
        RFAILEDP
    }
    if (append_frame(g_img, current_frame) != 0) {
        free(frame);
        free(current_frame);
        RFAILEDP
    }
    g_crt_frame++;
    return 0;
}

static int handle_gce(void) {
    uint8_t gce_block[6];
    uint8_t block_size;
    uint8_t packed_gce;
    uint16_t delay_time;
    uint8_t transp_index;
    uint8_t terminator;
    size_t bytesRead;
    bytesRead = compat_read(g_fd, &gce_block, 6);
    if (bytesRead < 6) RFAILEDP
        block_size = gce_block[0];
    if (block_size != 4) RFAILEDP
        packed_gce = gce_block[1];
    delay_time = gce_block[2] | (gce_block[3] << 8);
    transp_index = gce_block[4];
    terminator = gce_block[5];
    if (terminator != 0) RFAILEDP
        if (delay_time < 1) delay_time = 10;
        g_current_gce.delay_time = delay_time;
    g_current_gce.transparent_index = transp_index;
    g_current_gce.disposal_method = (packed_gce >> 2) & 7;
    g_current_gce.transparent_color_flag = packed_gce & 0x1;
    return 0;
}

static int handle_extension(void) {
    uint8_t extension_identifier;
    size_t bytesRead;
    bytesRead = compat_read(g_fd, &extension_identifier, 1);
    if (bytesRead < 1) RFAILEDP
        switch (extension_identifier) {
            case GRAPHIC_CONTROL_EXTENSION:
                return handle_gce();
            case COMMENT_EXTENSION:
                seek_through_blocks(g_fd);
                break;
            case PLAIN_TEXT_EXTENSION:
                fseek(g_fd, 13, SEEK_CUR);
                seek_through_blocks(g_fd);
                break;
            case APPLICATION_EXTENSION:
                fseek(g_fd, 12, SEEK_CUR);
                seek_through_blocks(g_fd);
                break;
            default:
                seek_through_blocks(g_fd);
                break;
        }
        return 0;
}

struct imageV2* parse(const char *filename)
{
    debugf(DEBUG_VERBOSE, "Trying to load GIF %s.", filename);
    size_t bytesRead;
    uint8_t ver[6];
    uint8_t lsd[7];
    uint8_t packed;
    uint8_t gct_flag;
    int gct_entries;
    uint8_t byte;
    g_img = malloc(sizeof(struct imageV2));
    if (!g_img) return 0;
    g_crt_delay = 0;
    g_crt_frame = 0;
    g_current_gce.delay_time = 0;
    g_current_gce.transparent_index = 0;
    g_current_gce.disposal_method = 0;
    g_current_gce.transparent_color_flag = 0;
    g_current_disposal = DISPOSE_NONE;
    g_fd = fopen(filename, "rb");
    if (!g_fd) {
        perror("Failed to open file");
        free(g_img);
        return 0;
    }
    g_img->frame_count = 0;
    g_img->frames = NULL;
    bytesRead = compat_read(g_fd, ver, sizeof(ver));
    if (bytesRead < 6 || (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0)) {
        free(g_img);
        RFAILEDP
    }
    bytesRead = compat_read(g_fd, lsd, sizeof(lsd));
    if (bytesRead < 7) {
        free(g_img);
        RFAILEDP
    }
    g_width = lsd[0] | (lsd[1] << 8);
    g_height = lsd[2] | (lsd[3] << 8);
    packed = lsd[4];
    gct_flag = packed >> 7;
    gct_entries = 1 << ((packed & 0x7) + 1);
    g_bg_index = lsd[5];
    if (gct_flag) process_palette(g_fd, gct_entries, g_global_palette);
    //if (g_bg_index < 256) g_global_palette[g_bg_index] = 0x00000000;
    while (1) {
        bytesRead = compat_read(g_fd, &byte, 1);
        if (bytesRead < 1) {
            free(g_img);
            RFAILEDP
        }
        if (byte == 0x3B) {
            break;
        } else if (byte == IMAGE_DESCRIPTOR) {
            if (handle_image_descriptor() != 0) {
                free(g_img);
                RFAILEDP
            }
            g_current_disposal = g_current_gce.disposal_method;
        } else if (byte == EXTENSION_DESCRIPTOR) {
            if (handle_extension() != 0) {
                free(g_img);
                RFAILEDP
            }
        }
    }
    debugf(DEBUG_VERBOSE, "Loaded GIF %s: %u frames, image size %u by %u pixels.", filename, g_img->frame_count, g_width, g_height);
    fclose(g_fd);
    g_img->width = g_width;
    g_img->height = g_height;
    return g_img;
}

void draw_imageV2(struct imageV2* img, uint32_t frameId, struct copydata* dst) {
    if (!img || !dst || !dst->pixels) return;
    if (!img->frames || frameId >= img->frame_count) return;
    if (!img->frames[frameId] || !img->frames[frameId]->pixels) return;
    if (dst->width == 0 || dst->height == 0) return;

    float xScale = (float)img->width / (float)dst->width;
    float yScale = (float)img->height / (float)dst->height;

    struct frameV2* f = img->frames[frameId];

    for (uint32_t dy = 0; dy < dst->height; dy++) {
        uint32_t srcY = (uint32_t)(dy * yScale);
        if (srcY >= img->height) srcY = img->height - 1;

        for (uint32_t dx = 0; dx < dst->width; dx++) {
            uint32_t srcX = (uint32_t)(dx * xScale);
            if (srcX >= img->width) srcX = img->width - 1;

            uint32_t srcIndex = srcY * img->width + srcX;
            uint8_t pixelId = f->pixels[srcIndex];
            uint32_t srcColor = f->palette[pixelId];
            uint8_t alpha = (srcColor >> 24) & 0xFF;
            if (alpha == 0) continue;

            uint32_t dstX = dst->x + dx;
            uint32_t dstY = dst->y + dy;
            if (dstX >= dst->bufferWidth || dstY >= dst->bufferHeight) continue;

            dst->pixels[dstY * dst->bufferWidth + dstX] = srcColor;
        }
    }
}
