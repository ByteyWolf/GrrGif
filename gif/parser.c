#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gif.h"
#include "../lzw/lzw.h"
#include "../image32.h"


size_t read(FILE* fd, void *buf, size_t count) {
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

    bytesRead = read(fd, gct, gct_size);
    if (bytesRead < gct_size) {
        free(gct);
        RFAILEDP
    }

    for (i = 0; i < num_entries; i++) {
        palette[i] = (0xFFu << 24) | ((uint32_t)gct[3*i] << 16) | ((uint32_t)gct[3*i + 1] << 8) | (uint32_t)gct[3*i + 2];
    }

    free(gct);
    return palette;
}

uint16_t calc_row_interlaced(uint16_t row_index, uint16_t height) {
    uint16_t row;
    uint16_t pass1_end;
    uint16_t pass2_end;
    uint16_t pass3_end;
    
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

int append_frame(struct image32* img, struct frame32* new_frame) {
    struct frame32** temp;
    
    if (!img || !new_frame) return -1;

    temp = realloc(img->frames, (img->frame_count + 1) * sizeof(struct frame32*));
    if (!temp) return -1;

    img->frames = temp;
    img->frames[img->frame_count] = new_frame;
    img->frame_count++;

    return 0;
}

void seek_through_blocks(int fd) {
    uint8_t block_size;
    long bytesRead;
    
    while (1) {
        bytesRead = read(fd, &block_size, 1);
        if (bytesRead < 1) RFAILED
        if (block_size == 0) break;
        fseek(fd, block_size, SEEK_CUR);
    }
}


/* until we can come up with a proper structure */
struct image32* parse(const char *filename)
{
    uint32_t global_palette[256];
    uint32_t local_palette[256];
    struct image32* img;
    uint32_t crt_delay;
    uint32_t crt_frame;
    struct gce_data current_gce;
    uint8_t current_disposal;
    FILE *fd;
    size_t bytesRead;
    uint8_t ver[6];
    uint8_t lsd[7];
    uint16_t width, height;
    uint8_t packed;
    uint8_t gct_flag;
    int color_res;
    int gct_entries;
    uint8_t bg_index;
    uint8_t pixel_aspect;
    uint8_t byte;
    
    img = malloc(sizeof(struct image32));
    crt_delay = 0;
    crt_frame = 0;
    current_gce.delay_time = 0;
    current_gce.transparent_index = 0;
    current_gce.disposal_method = 0;
    current_gce.transparent_color_flag = 0;
    current_disposal = DISPOSE_NONE;
    
    fd = fopen(filename, "rb");

    img->frame_count = 0;
    img->frames = NULL;

    if (!fd) {
        perror("Failed to open file");
        return 0;
    }

    /* header */
    bytesRead = read(fd, ver, sizeof(ver));
    if (bytesRead < 6 || (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0)) RFAILEDP

    /* logical screen descriptor or otherwise lsd (not the other lsd) */
    bytesRead = read(fd, lsd, sizeof(lsd));
    if (bytesRead < 7) RFAILEDP

    width  = lsd[0] | (lsd[1] << 8);
    height = lsd[2] | (lsd[3] << 8);

    packed = lsd[4];
    gct_flag = packed >> 7;
    color_res = ((packed >> 4) & 0x7) + 1;
    /*uint8_t sort_flag = (packed >> 3) & 1;*/
    gct_entries = 1 << ((packed & 0x7) + 1);

    bg_index = lsd[5];
    pixel_aspect = lsd[6];

    /*uint32_t* canvas = malloc(width * height * sizeof(uint32_t));*/
    if (gct_flag) process_palette(fd, gct_entries, global_palette);

    /* now read until we find the trailer */
    while (1) {
        bytesRead = read(fd, &byte, 1);
        if (bytesRead < 1) RFAILEDP
        if (byte == 0x3B) {
            printf("Reached trailer normally.\n");
            break;
        } else if (byte == IMAGE_DESCRIPTOR) {
            uint8_t img_desc[9];
            uint16_t img_left;
            uint16_t img_top;
            uint16_t local_width;
            uint16_t local_height;
            uint8_t img_packed;
            uint8_t lct_flag;
            uint8_t interlace_flag;
            uint8_t sort_flag;
            int lct_entries;
            struct frame32 *current_frame;
            uint32_t* frame;
            uint8_t lzw_min_code_size;
            lzw_state_t decoder;
            uint32_t pixel_index;
            
            bytesRead = read(fd, img_desc, sizeof(img_desc));
            if (bytesRead < 9) RFAILEDP
            img_left   = img_desc[0] | (img_desc[1] << 8);
            img_top    = img_desc[2] | (img_desc[3] << 8);
            local_width  = img_desc[4] | (img_desc[5] << 8);
            local_height = img_desc[6] | (img_desc[7] << 8);
            img_packed = img_desc[8];
            lct_flag = img_packed >> 7;
            interlace_flag = (img_packed >> 6) &  1;
            sort_flag = (img_packed >> 5) & 1;
            lct_entries = 1 << ((img_packed & 0x7) + 1);
            if (lct_flag) {
                process_palette(fd, lct_entries, local_palette);
            } else {
                memcpy(local_palette, global_palette, sizeof(global_palette));
            }

            /* is rendering time!! */
            current_frame = malloc(sizeof(struct frame32));
            frame = malloc(width * height * sizeof(uint32_t));
            memset(frame, 0, width * height * sizeof(uint32_t));
            current_frame->pixels = frame;
            current_frame->delay = crt_delay;
            crt_delay += current_gce.delay_time * 10;
            bytesRead = read(fd, &lzw_min_code_size, 1);
            if (bytesRead < 1) RFAILEDP

            if (!frame) RFAILEDP

            lzw_init(&decoder, lzw_min_code_size);
            pixel_index = 0;
            

            while (1) {
                uint8_t sub_block_size;
                uint8_t* sub_block_data;
                size_t outSize;
                const unsigned char* chunk;
                int px;
                
                bytesRead = read(fd, &sub_block_size, 1);
                if (bytesRead < 1) {
                    free(frame); free(img);
                    RFAILEDP
                }
                if (sub_block_size == 0) break;
                sub_block_data = malloc(sub_block_size);
                if (!sub_block_data) { free(frame); free(img); RFAILEDP }
                bytesRead = read(fd, sub_block_data, sub_block_size);
                if (bytesRead < sub_block_size) { free(sub_block_data); free(frame); free(img); RFAILEDP }

                /*for (int i = 0; i < sub_block_size; i++) {*/
                chunk = lzw_feed(&decoder, sub_block_data, sub_block_size, &outSize);

                /* iterate through decoded bytes and write them into canvas or something */
                for (px = 0; px<outSize; px++) {
                    uint32_t pixeloff;
                    uint16_t inner_row;
                    uint16_t inner_col;
                    uint16_t outer_row;
                    uint16_t outer_col;
                    int transp_idx;
                    uint32_t color;
                    
                    pixeloff = pixel_index;
                    inner_row = pixel_index / local_width;
                    inner_col = pixel_index % local_width;
                    outer_row = img_top + inner_row;
                    outer_col = img_left + inner_col;

                    if (interlace_flag) {
                        outer_row = img_top + calc_row_interlaced(inner_row, local_height);
                    }

                    pixeloff = outer_row * width + outer_col;

                    transp_idx = current_gce.transparent_color_flag ? current_gce.transparent_index : -1;

                    /* here we write into the canvas based on the mode given */
                    color = local_palette[chunk[px]];
                    if (chunk[px] == bg_index) color = 0;
                    if (pixeloff >= width * height) {
                        free(sub_block_data); free(frame); free(img); RFAILEDP
                    }
                    switch (current_disposal) {
                        case DISPOSE_NONE:
                        case DISPOSE_UNSPECIFIED:
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            break;
                        case DISPOSE_BACKGROUND:
                            /* only fill the transparent pixels with bg color i think */
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            else frame[pixeloff] = 0; /*local_palette[bg_index];*/
                            break;
                        case DISPOSE_PREVIOUS:
                            {
                            uint32_t* prev_frame;
                            if (crt_frame == 0) { free(frame); free(img); RFAILEDP }
                            prev_frame = img->frames[crt_frame - 1]->pixels;
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            else frame[pixeloff] = prev_frame[pixeloff];
                            }
                            break;
                    }

                    pixel_index++;
                }
                /*}*/
                free(sub_block_data);
            }
            printf("%u: written %u pixels\n", crt_frame, pixel_index);
            current_disposal = current_gce.disposal_method;
            append_frame(img, current_frame);
            crt_frame++;
            
        } else if (byte == EXTENSION_DESCRIPTOR) {
            uint8_t extension_identifier;
            
            bytesRead = read(fd, &extension_identifier, 1);
            if (bytesRead < 1) { free(img); RFAILEDP }
            switch (extension_identifier) {
                case GRAPHIC_CONTROL_EXTENSION:
                    {
                    uint8_t gce_block[6];
                    uint8_t block_size;
                    uint8_t packed_gce;
                    uint16_t delay_time;
                    uint8_t transp_index;
                    uint8_t terminator;
                    
                    bytesRead = read(fd, &gce_block, 5+1);

                    block_size = gce_block[0];
                    if (block_size != 4) RFAILEDP
                    packed_gce = gce_block[1];
                    delay_time = gce_block[2] | (gce_block[3] << 8);
                    transp_index = gce_block[4];
                    terminator = gce_block[5];
                    if (terminator != 0) RFAILEDP
                    if (delay_time < 1) delay_time = 10;
                    
                    current_gce.delay_time = delay_time;
                    current_gce.transparent_index = transp_index;
                    current_gce.disposal_method = (packed_gce >> 2) & 7;
                    current_gce.transparent_color_flag = packed_gce & 0x1;
                    printf("Frame %u: delay %dms, transp idx %d, disposal %d, transp flag %d\n", crt_frame, delay_time*10, transp_index, current_gce.disposal_method, current_gce.transparent_color_flag);
                    }
                    break;
                case COMMENT_EXTENSION:
                    seek_through_blocks(fd);
                    break;
                case PLAIN_TEXT_EXTENSION:
                    fseek(fd, 13, SEEK_CUR); /* block size + 12 bytes of data */
                    seek_through_blocks(fd);
                    break;
                case APPLICATION_EXTENSION:
                    fseek(fd, 12, SEEK_CUR);
                    seek_through_blocks(fd);
                    break;
                default:
                    /* skip other extensions for now */
                    while (1) {
                        uint8_t sub_block_size;
                        bytesRead = read(fd, &sub_block_size, 1);
                        if (bytesRead < 1) { free(img); RFAILEDP }
                        if (sub_block_size == 0) break;
                        /* skip the data */
                        fseek(fd, sub_block_size, SEEK_CUR);
                    }
                    continue;
            }
        } 
    }

    fclose(fd);
    img->width = width;
    img->height = height;
    return img;
}