#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gif.h"
#include "../lzw/lzw.h"
#include "../image32.h"
#include "../debug.h"

#define FAIL_GIF {fclose(ctx.fd); free(ctx.img); return NULL;}

typedef struct {
    uint32_t delay_time;
    uint8_t transp_idx;
    uint8_t disposal_method;
    uint8_t transp_flag;
} gce_data_t;

typedef struct {
    uint32_t global_palette[256];
    uint32_t local_palette[256];
    uint8_t gct_entries;
    uint8_t lct_entries;
    
    uint32_t file_width;
    uint32_t file_height;

    uint32_t local_top;
    uint32_t local_left;
    uint32_t local_width;
    uint32_t local_height;

    uint8_t interlace_flag;

    uint8_t bg_index;

    gce_data_t current_gce;

    struct imageV2* img;
    FILE* fd;
} gif_context_t;

static void seek_through_blocks(FILE* fd) {
    uint8_t block_size;
    while (read(&block_size, 1, 1, fd) && block_size != 0) {
        fseek(fd, block_size, SEEK_CUR);
    }
}

static uint8_t process_palette(FILE* fd, uint8_t num_entries, uint32_t* palette)
{
    if (num_entries == 0) return 0;

    size_t gct_size = 3 * num_entries;
    uint8_t *gct = (uint8_t *)malloc(gct_size);
    if (!gct) return 0;

    if (!read(gct, gct_size, 1, fd)) {
        free(gct);
        return 0;
    }

    for (int i = 0; i < num_entries; i++) {
        uint8_t *rgb = &gct[3*i];
        palette[i] = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    }

    free(gct);
    return 1;
}

static uint8_t handle_gce(gif_context_t* ctx) {
    uint8_t gce_block[6];

    if (compat_read(ctx->fd, gce_block, 6) < 6) return 0;
    if (gce_block[0] != 4 || gce_block[5] != 0) return 0;

    uint8_t packed_gce = gce_block[1];
    uint16_t delay_time = gce_block[2] | (gce_block[3] << 8);

    if (delay_time < 1) delay_time = 10;

    ctx->current_gce.delay_time = delay_time;
    ctx->current_gce.transp_idx = gce_block[4];
    ctx->current_gce.disposal_method = (packed_gce >> 2) & 7;
    ctx->current_gce.transp_flag = packed_gce & 0x1;

    return 1;
}

static uint8_t process_lzw_block(gif_context_t* ctx, lzw_state_t* decoder, uint8_t* frame, uint32_t* pixel_index) {
    uint8_t sub_block_size;

    if (!read(&sub_block_size, 1, 1, ctx->fd)) return 0;
    if (sub_block_size == 0) return 1;

    uint8_t* sub_block_data = malloc(sub_block_size);
    if (!sub_block_data) return 0;

    if (!read(sub_block_data, sub_block_size, 1, ctx->fd)) {
        free(sub_block_data);
        return 0;
    }

    size_t outSize;
    const unsigned char* chunk = lzw_feed(decoder, sub_block_data, sub_block_size, &outSize);

    for (size_t px = 0; px < outSize; px++) {
        if (process_pixel(ctx, frame, *pixel_index, chunk[px]) != 0) {
            free(sub_block_data);
            return 0;
        }
        (*pixel_index)++;
    }

    free(sub_block_data);
    return 2;
}

static uint8_t decode_image_data(gif_context_t* ctx, uint8_t* frame) {
    uint8_t lzw_min_code_size;
    if (!read(&lzw_min_code_size, 1, 1, ctx->fd)) return 0;

    lzw_state_t* decoder = malloc(sizeof(lzw_state_t));
    if (!decoder) return 0;

    lzw_init(decoder, lzw_min_code_size);

    uint32_t pixel_index = 0;
    int result;
    while ((result = process_lzw_block(ctx, decoder, frame, &pixel_index)) > 1);

    free(decoder);
    return (result == 0) ? 0 : 1;
}

static uint8_t handle_image_descriptor(gif_context_t* ctx) {
    uint8_t img_desc[9];

    if (!read(img_desc, 9, 1, ctx->fd)) return 0;

    ctx->local_left = img_desc[0] | (img_desc[1] << 8);
    ctx->local_top = img_desc[2] | (img_desc[3] << 8);
    ctx->local_width = img_desc[4] | (img_desc[5] << 8);
    ctx->local_height = img_desc[6] | (img_desc[7] << 8);

    uint8_t img_packed = img_desc[8];
    uint8_t lct_flag = img_packed >> 7;
    ctx->interlace_flag = (img_packed >> 6) & 1;

    if (lct_flag) {
        ctx->lct_entries = 1 << ((img_packed & 0x7) + 1);
        if (process_palette(ctx->fd, ctx->lct_entries, ctx->local_palette) != 0) return -1;
    } else {
        memcpy(ctx->local_palette, ctx->global_palette, ctx->gct_entries);
        ctx->lct_entries = ctx->gct_entries;
    }

    struct frameV2* current_frame = malloc(sizeof(struct frameV2));
    uint8_t* frame = calloc(ctx->width * ctx->height, sizeof(uint8_t));

    if (!frame || !current_frame) {
        free(frame);
        free(current_frame);
        return 1;
    }

    /* note: if we're using gct then background is bg_index,
    otherwise background is transp_idx. */

    uint8_t bg_index_local = lct_flag ? ctx->current_gce.transp_idx : ctx->bg_index;

    // pre-fill
    if (ctx->crt_frame > 0) {
        if (ctx->current_disposal == DISPOSE_BACKGROUND) {
            memset(frame, bg_index_local, ctx->width * ctx->height);
        } else if (ctx->current_disposal == DISPOSE_PREVIOUS) {
            memcpy(frame, ctx->img->frames[ctx->crt_frame - 1]->pixels,
                   ctx->width * ctx->height);
        } else {
            memcpy(frame, ctx->img->frames[ctx->crt_frame - 1]->pixels,
                   ctx->width * ctx->height);
        }
    } else {
        memset(frame, bg_index_local, ctx->width * ctx->height);
    }

    current_frame->pixels = frame;
    current_frame->delay = ctx->crt_delay;
    current_frame->transp_idx = bg_index_local;

    memcpy(current_frame->palette, ctx->local_palette, sizeof(current_frame->palette));
    if (ctx->current_gce.transparent_color_flag) {
        current_frame->palette[bg_index_local] &= 0x00FFFFFF;
    }

    if (decode_image_data(ctx, frame) != 0) {
        free(frame);
        free(current_frame);
        return 0;
    }

    if (append_frame(ctx->img, current_frame) != 0) {
        free(frame);
        free(current_frame);
        return 0;
    }

    ctx->crt_frame++;
    return 1;
}


struct imageV2* parse(const char *filename) {
    debugf(DEBUG_VERBOSE, "Trying to load GIF %s.", filename);
    gif_context_t ctx = {0};

    ctx.img = malloc(sizeof(struct imageV2));
    if (!ctx.img) return NULL;

    ctx.fd = fopen(filename, "rb");
    if (!ctx.fd) {
        perror("Failed to open file");
        free(ctx.img);
        return NULL;
    }

    // ===== GIF HEADER =====

    uint8_t ver[6];
    if (!read(ver, 6, 1, ctx.fd) ||
        (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0))
        FAIL_GIF

    uint8_t lsd[7];
    if (!read(lsd, 7, 1, ctx.fd)) FAIL_GIF

    ctx.file_width = lsd[0] | (lsd[1] << 8);
    ctx.file_height = lsd[2] | (lsd[3] << 8);
    uint8_t packed = lsd[4];
    ctx.bg_index = lsd[5];

    if (packed & 0x80) {
        ctx.gct_entries = 1 << ((packed & 0x7) + 1);
        if (process_palette(ctx.fd, ctx.gct_entries, ctx.global_palette) != 0)
            FAIL_GIF
    }

    // ===== GIF BLOCKS =====
    uint8_t byte;
    while (read(&byte, 1, 1, ctx.fd)) {
        if (byte == 0x3B) {
            break;
            
        } else if (byte == IMAGE_DESCRIPTOR) {
            if (!handle_image_descriptor(&ctx)) FAIL_GIF
            
        } else if (byte == EXTENSION_DESCRIPTOR) {
            uint8_t extension_identifier;

            if (!read(&extension_identifier, 1, 1, ctx->fd)) FAIL_GIF
            switch (extension_identifier) {
                case GRAPHIC_CONTROL_EXTENSION:
                    if (!handle_gce(ctx)) FAIL_GIF;
                    break;
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
        }
    }

    debugf(DEBUG_VERBOSE, "Loaded GIF %s: %u frames, image size %u by %u pixels.",
               filename, ctx.img->frame_count, ctx.width, ctx.height);

    fclose(ctx.fd);
    ctx.img->width = ctx.width;
    ctx.img->height = ctx.height;
    return ctx.img;
}

    

