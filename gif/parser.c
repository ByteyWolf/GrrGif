#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gif.h"
#include "../lzw/lzw.h"
#include "../image32.h"
#include "../debug.h"

typedef struct {
    uint32_t global_palette[256];
    uint32_t local_palette[256];
    uint32_t width;
    uint32_t height;
    uint32_t img_left;
    uint32_t img_top;
    uint32_t local_width;
    uint32_t local_height;
    uint8_t interlace_flag;
    uint8_t bg_index;
    uint8_t current_disposal;
    int transp_idx;
    struct gce_data current_gce;
    uint32_t crt_delay;
    uint32_t crt_frame;
    struct imageV2* img;
    FILE* fd;
} gif_context_t;

static inline size_t compat_read(FILE* fd, void *buf, size_t count) {
    return fread(buf, 1, count, fd);
}

static int process_palette(FILE* fd, int num_entries, uint32_t* palette)
{
    if (num_entries <= 0 || num_entries > 256) return -1;

    size_t gct_size = 3 * num_entries;
    uint8_t *gct = (uint8_t *)malloc(gct_size);
    if (!gct) return -1;

    size_t bytesRead = compat_read(fd, gct, gct_size);
    if (bytesRead < gct_size) {
        free(gct);
        return -1;
    }

    // Optimize: process palette in single loop
    for (int i = 0; i < num_entries; i++) {
        uint8_t *rgb = &gct[3*i];
        palette[i] = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    }

    free(gct);
    return 0;
}

static inline uint32_t calc_row_interlaced_uint(uint32_t row_index, uint32_t height) {
    uint32_t pass1_end = (height + 7) >> 3;  // Faster division by 8
    uint32_t pass2_end = (height + 3) >> 2;  // Faster division by 4
    uint32_t pass3_end = (height + 1) >> 1;  // Faster division by 2

    if (row_index < pass1_end)
        return row_index << 3;  // * 8
        else if (row_index < pass2_end)
            return ((row_index - pass1_end) << 3) + 4;
    else if (row_index < pass3_end)
        return ((row_index - pass2_end) << 2) + 2;
    else
        return ((row_index - pass3_end) << 1) + 1;
}

static int append_frame(struct imageV2* img, struct frameV2* new_frame) {
    if (!img || !new_frame) return -1;

    struct frameV2** temp = realloc(img->frames, (img->frame_count + 1) * sizeof(struct frameV2*));
    if (!temp) return -1;

    img->frames = temp;
    img->frames[img->frame_count++] = new_frame;
    return 0;
}

static void seek_through_blocks(FILE* fd) {
    uint8_t block_size;
    while (compat_read(fd, &block_size, 1) == 1 && block_size != 0) {
        fseek(fd, block_size, SEEK_CUR);
    }
}

static int process_pixel(gif_context_t* ctx, uint8_t* frame, uint32_t pixel_index, uint8_t color_index) {
    uint32_t inner_row = pixel_index / ctx->local_width;
    uint32_t inner_col = pixel_index % ctx->local_width;

    uint32_t outer_row = ctx->img_top + (ctx->interlace_flag ?
    calc_row_interlaced_uint(inner_row, ctx->local_height) : inner_row);
    uint32_t outer_col = ctx->img_left + inner_col;

    if (outer_row >= ctx->height || outer_col >= ctx->width) return -1;

    uint32_t pixeloff = outer_row * ctx->width + outer_col;

    int is_transparent = (ctx->transp_idx >= 0 && color_index == (uint8_t)ctx->transp_idx);

    switch (ctx->current_disposal) {
        case DISPOSE_NONE:
        case DISPOSE_UNSPECIFIED:
            if (!is_transparent) {
                frame[pixeloff] = color_index;
            }
            else if (ctx->crt_frame == 0) {
                frame[pixeloff] = ctx->bg_index;
            }
            break;

        case DISPOSE_BACKGROUND:
            frame[pixeloff] = is_transparent ? ctx->bg_index : color_index;
            break;

        case DISPOSE_PREVIOUS:
            if (ctx->crt_frame == 0) {
                frame[pixeloff] = is_transparent ? ctx->bg_index : color_index;
            } else {
                uint8_t* prev_frame = ctx->img->frames[ctx->crt_frame - 1]->pixels;
                frame[pixeloff] = is_transparent ? prev_frame[pixeloff] : color_index;
            }
            break;
    }

    return 0;
}

static int process_lzw_block(gif_context_t* ctx, lzw_state_t* decoder, uint8_t* frame, uint32_t* pixel_index) {
    uint8_t sub_block_size;

    if (compat_read(ctx->fd, &sub_block_size, 1) < 1) return -1;
    if (sub_block_size == 0) return 0;

    uint8_t* sub_block_data = malloc(sub_block_size);
    if (!sub_block_data) return -1;

    if (compat_read(ctx->fd, sub_block_data, sub_block_size) < sub_block_size) {
        free(sub_block_data);
        return -1;
    }

    size_t outSize;
    const unsigned char* chunk = lzw_feed(decoder, sub_block_data, sub_block_size, &outSize);

    for (size_t px = 0; px < outSize; px++) {
        if (process_pixel(ctx, frame, *pixel_index, chunk[px]) != 0) {
            free(sub_block_data);
            return -1;
        }
        (*pixel_index)++;
    }

    free(sub_block_data);
    return 1;
}

static int decode_image_data(gif_context_t* ctx, uint8_t* frame) {
    uint8_t lzw_min_code_size;
    if (compat_read(ctx->fd, &lzw_min_code_size, 1) < 1) return -1;

    lzw_state_t* decoder = malloc(sizeof(lzw_state_t));
    if (!decoder) return -1;

    lzw_init(decoder, lzw_min_code_size);

    uint32_t pixel_index = 0;
    int result;
    while ((result = process_lzw_block(ctx, decoder, frame, &pixel_index)) > 0);

    free(decoder);
    return (result < 0) ? -1 : 0;
}

static int read_image_descriptor(gif_context_t* ctx) {
    uint8_t img_desc[9];

    if (compat_read(ctx->fd, img_desc, 9) < 9) return -1;

    ctx->img_left = img_desc[0] | (img_desc[1] << 8);
    ctx->img_top = img_desc[2] | (img_desc[3] << 8);
    ctx->local_width = img_desc[4] | (img_desc[5] << 8);
    ctx->local_height = img_desc[6] | (img_desc[7] << 8);

    uint8_t img_packed = img_desc[8];
    uint8_t lct_flag = img_packed >> 7;
    ctx->interlace_flag = (img_packed >> 6) & 1;

    if (lct_flag) {
        int lct_entries = 1 << ((img_packed & 0x7) + 1);
        if (process_palette(ctx->fd, lct_entries, ctx->local_palette) != 0) return -1;
    } else {
        memcpy(ctx->local_palette, ctx->global_palette, 256 * sizeof(uint32_t));
    }

    return 0;
}

static int handle_image_descriptor(gif_context_t* ctx) {
    if (read_image_descriptor(ctx) != 0) return -1;

    struct frameV2* current_frame = malloc(sizeof(struct frameV2));
    uint8_t* frame = calloc(ctx->width * ctx->height, sizeof(uint8_t));

    if (!frame || !current_frame) {
        free(frame);
        free(current_frame);
        return -1;
    }

    if (ctx->crt_frame > 0) {
        if (ctx->current_disposal == DISPOSE_BACKGROUND) {
            memset(frame, ctx->bg_index, ctx->width * ctx->height);
        } else if (ctx->current_disposal == DISPOSE_PREVIOUS) {
            memcpy(frame, ctx->img->frames[ctx->crt_frame - 1]->pixels,
                   ctx->width * ctx->height);
        } else {
            memcpy(frame, ctx->img->frames[ctx->crt_frame - 1]->pixels,
                   ctx->width * ctx->height);
        }
    } else {
        memset(frame, ctx->bg_index, ctx->width * ctx->height);
    }

    current_frame->pixels = frame;
    current_frame->delay = ctx->crt_delay;

    memcpy(current_frame->palette, ctx->local_palette, sizeof(current_frame->palette));
    if (ctx->current_gce.transparent_color_flag) {
        uint8_t transp_idx = ctx->current_gce.transparent_index;
        current_frame->palette[transp_idx] &= 0x00FFFFFF;
    }

    ctx->crt_delay += ctx->current_gce.delay_time * 10;
    ctx->transp_idx = ctx->current_gce.transparent_color_flag ?
    ctx->current_gce.transparent_index : -1;

    if (decode_image_data(ctx, frame) != 0) {
        free(frame);
        free(current_frame);
        return -1;
    }

    if (append_frame(ctx->img, current_frame) != 0) {
        free(frame);
        free(current_frame);
        return -1;
    }

    ctx->crt_frame++;
    return 0;
}

static int handle_gce(gif_context_t* ctx) {
    uint8_t gce_block[6];

    if (compat_read(ctx->fd, gce_block, 6) < 6) return -1;
    if (gce_block[0] != 4 || gce_block[5] != 0) return -1;

    uint8_t packed_gce = gce_block[1];
    uint16_t delay_time = gce_block[2] | (gce_block[3] << 8);

    if (delay_time < 1) delay_time = 10;

    ctx->current_gce.delay_time = delay_time;
    ctx->current_gce.transparent_index = gce_block[4];
    ctx->current_gce.disposal_method = (packed_gce >> 2) & 7;
    ctx->current_gce.transparent_color_flag = packed_gce & 0x1;

    return 0;
}

static int handle_extension(gif_context_t* ctx) {
    uint8_t extension_identifier;

    if (compat_read(ctx->fd, &extension_identifier, 1) < 1) return -1;

    switch (extension_identifier) {
        case GRAPHIC_CONTROL_EXTENSION:
            return handle_gce(ctx);
        case COMMENT_EXTENSION:
            seek_through_blocks(ctx->fd);
            break;
        case PLAIN_TEXT_EXTENSION:
            fseek(ctx->fd, 13, SEEK_CUR);
            seek_through_blocks(ctx->fd);
            break;
        case APPLICATION_EXTENSION:
            fseek(ctx->fd, 12, SEEK_CUR);
            seek_through_blocks(ctx->fd);
            break;
        default:
            seek_through_blocks(ctx->fd);
            break;
    }

    return 0;
}

struct imageV2* parse(const char *filename)
{
    debugf(DEBUG_VERBOSE, "Trying to load GIF %s.", filename);

    gif_context_t ctx = {0};
    ctx.transp_idx = -1;
    ctx.current_disposal = DISPOSE_NONE;

    ctx.img = calloc(1, sizeof(struct imageV2));
    if (!ctx.img) return NULL;

    ctx.fd = fopen(filename, "rb");
    if (!ctx.fd) {
        perror("Failed to open file");
        free(ctx.img);
        return NULL;
    }

    uint8_t ver[6];
    if (compat_read(ctx.fd, ver, 6) < 6 ||
        (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0)) {
        fclose(ctx.fd);
    free(ctx.img);
    return NULL;
        }

        uint8_t lsd[7];
        if (compat_read(ctx.fd, lsd, 7) < 7) {
            fclose(ctx.fd);
            free(ctx.img);
            return NULL;
        }

        ctx.width = lsd[0] | (lsd[1] << 8);
        ctx.height = lsd[2] | (lsd[3] << 8);
        uint8_t packed = lsd[4];
        ctx.bg_index = lsd[5];

        if (packed & 0x80) {
            int gct_entries = 1 << ((packed & 0x7) + 1);
            if (process_palette(ctx.fd, gct_entries, ctx.global_palette) != 0) {
                fclose(ctx.fd);
                free(ctx.img);
                return NULL;
            }
        }

        uint8_t byte;
        while (compat_read(ctx.fd, &byte, 1) == 1) {
            if (byte == 0x3B) {
                break;
            } else if (byte == IMAGE_DESCRIPTOR) {
                if (handle_image_descriptor(&ctx) != 0) {
                    fclose(ctx.fd);
                    free(ctx.img);
                    return NULL;
                }
                ctx.current_disposal = ctx.current_gce.disposal_method;
            } else if (byte == EXTENSION_DESCRIPTOR) {
                if (handle_extension(&ctx) != 0) {
                    fclose(ctx.fd);
                    free(ctx.img);
                    return NULL;
                }
            }
        }

        debugf(DEBUG_VERBOSE, "Loaded GIF %s: %u frames, image size %u by %u pixels.",
               filename, ctx.img->frame_count, ctx.width, ctx.height);

        fclose(ctx.fd);
        ctx.img->width = ctx.width;
        ctx.img->height = ctx.height;
        return ctx.img;
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
