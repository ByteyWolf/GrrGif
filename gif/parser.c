#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gif.h"
#include "../lzw/lzw.h"
#include "../image32.h"


uint32_t* process_palette(int fd, int num_entries, uint32_t* palette)
{
    int bytesRead;
    size_t gct_size = 3 * num_entries;
    uint8_t *gct = (uint8_t *)malloc(gct_size);
    if (!gct) RFAILEDP

    bytesRead = read(fd, gct, gct_size);
    if (bytesRead < gct_size) {
        free(gct);
        RFAILEDP
    }

    for (int i = 0; i < num_entries; i++) {
        palette[i] = (0xFFu << 24) | ((uint32_t)gct[3*i] << 16) | ((uint32_t)gct[3*i + 1] << 8) | (uint32_t)gct[3*i + 2];
    }

    free(gct);
    return palette;
}

uint16_t calc_row_interlaced(uint16_t row_index, uint16_t height) {
    uint16_t row;
    uint16_t pass1_end = (height + 7)/8;
    uint16_t pass2_end = (height + 3)/4;
    uint16_t pass3_end = (height + 1)/2;

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
    if (!img || !new_frame) return -1;

    struct frame32** temp = realloc(img->frames, (img->frame_count + 1) * sizeof(struct frame32*));
    if (!temp) return -1;

    img->frames = temp;
    img->frames[img->frame_count] = new_frame;
    img->frame_count++;

    return 0;
}

void seek_through_blocks(int fd) {
    uint8_t block_size;
    ssize_t bytesRead;
    while (1) {
        bytesRead = read(fd, &block_size, 1);
        if (bytesRead < 1) RFAILED
        if (block_size == 0) break;
        lseek(fd, block_size, SEEK_CUR);
    }
}


// until we can come up with a proper structure
struct image32* parse(const char *filename)
{
    uint32_t global_palette[256];
    uint32_t local_palette[256];
    struct image32* img = malloc(sizeof(struct image32));
    img->frame_count = 0;
    img->frames = NULL;

    uint32_t crt_delay = 0;
    uint32_t crt_frame = 0;
    struct gce_data current_gce = {0, 0, 0, 0};
    uint8_t current_disposal = DISPOSE_NONE;
    
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return 0;
    }
    // header
    uint8_t ver[6];
    ssize_t bytesRead = read(fd, ver, sizeof(ver));
    if (bytesRead < 6 || (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0)) RFAILEDP

    // logical screen descriptor or otherwise lsd (not the other lsd)
    uint8_t lsd[7];
    bytesRead = read(fd, lsd, sizeof(lsd));
    if (bytesRead < 7) RFAILEDP

    uint16_t width  = lsd[0] | (lsd[1] << 8);
    uint16_t height = lsd[2] | (lsd[3] << 8);

    uint8_t packed = lsd[4];
    uint8_t gct_flag = packed >> 7;
    int color_res = ((packed >> 4) & 0x7) + 1;
    //uint8_t sort_flag = (packed >> 3) & 1;
    int gct_entries = 1 << ((packed & 0x7) + 1);

    uint8_t bg_index = lsd[5];
    uint8_t pixel_aspect = lsd[6];

    //uint32_t* canvas = malloc(width * height * sizeof(uint32_t));
    if (gct_flag) process_palette(fd, gct_entries, global_palette);

    // now read until we find the trailer
    uint8_t byte;
    while (1) {
        bytesRead = read(fd, &byte, 1);
        if (bytesRead < 1) RFAILEDP
        if (byte == 0x3B) {
            printf("Reached trailer normally.\n");
            break;
        } else if (byte == IMAGE_DESCRIPTOR) {
            uint8_t img_desc[9];
            bytesRead = read(fd, img_desc, sizeof(img_desc));
            if (bytesRead < 9) RFAILEDP
            uint16_t img_left   = img_desc[0] | (img_desc[1] << 8);
            uint16_t img_top    = img_desc[2] | (img_desc[3] << 8);
            uint16_t local_width  = img_desc[4] | (img_desc[5] << 8);
            uint16_t local_height = img_desc[6] | (img_desc[7] << 8);
            uint8_t img_packed = img_desc[8];
            uint8_t lct_flag = img_packed >> 7;
            uint8_t interlace_flag = (img_packed >> 6) &  1;
            uint8_t sort_flag = (img_packed >> 5) & 1;
            int lct_entries = 1 << ((img_packed & 0x7) + 1);
            if (lct_flag) {
                process_palette(fd, lct_entries, local_palette);
            } else {
                memcpy(local_palette, global_palette, sizeof(global_palette));
            }

            // is rendering time!!
            struct frame32 *current_frame = malloc(sizeof(struct frame32));
            uint32_t* frame = malloc(width * height * sizeof(uint32_t));
            memset(frame, 0, width * height * sizeof(uint32_t));
            current_frame->pixels = frame;
            current_frame->delay = crt_delay;
            crt_delay += current_gce.delay_time * 10;
            uint8_t lzw_min_code_size;
            bytesRead = read(fd, &lzw_min_code_size, 1);
            if (bytesRead < 1) RFAILEDP

            if (!frame) RFAILEDP

            lzw_state_t decoder;
            lzw_init(&decoder, lzw_min_code_size);
            uint32_t pixel_index = 0;
            

            while (1) {
                uint8_t sub_block_size;
                bytesRead = read(fd, &sub_block_size, 1);
                if (bytesRead < 1) {
                    free(frame); free(img);
                    RFAILEDP
                }
                if (sub_block_size == 0) break;
                uint8_t* sub_block_data = malloc(sub_block_size);
                if (!sub_block_data) { free(frame); free(img); RFAILEDP }
                bytesRead = read(fd, sub_block_data, sub_block_size);
                if (bytesRead < sub_block_size) { free(sub_block_data); free(frame); free(img); RFAILEDP }

                //for (int i = 0; i < sub_block_size; i++) {
                size_t outSize;
                const unsigned char* chunk = lzw_feed(&decoder, sub_block_data, sub_block_size, &outSize);

                // iterate through decoded bytes and write them into canvas or something
                for (int px = 0; px<outSize; px++) {
                    uint32_t pixeloff = pixel_index;
                    uint16_t inner_row = pixel_index / local_width;
                    uint16_t inner_col = pixel_index % local_width;
                    uint16_t outer_row = img_top + inner_row;
                    uint16_t outer_col = img_left + inner_col;

                    if (interlace_flag) {
                        outer_row = img_top + calc_row_interlaced(inner_row, local_height);
                    }

                    pixeloff = outer_row * width + outer_col;

                    int transp_idx = current_gce.transparent_color_flag ? current_gce.transparent_index : -1;

                    // here we write into the canvas based on the mode given
                    uint32_t color = local_palette[chunk[px]];
                    if (chunk[px] == bg_index) color = 0;
                    switch (current_disposal) {
                        case DISPOSE_NONE:
                        case DISPOSE_UNSPECIFIED:
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            break;
                        case DISPOSE_BACKGROUND:
                            // only fill the transparent pixels with bg color i think
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            else frame[pixeloff] = 0; //local_palette[bg_index];
                            break;
                        case DISPOSE_PREVIOUS:
                            if (crt_frame == 0) { free(frame); free(img); RFAILEDP }
                            uint32_t* prev_frame = img->frames[crt_frame - 1]->pixels;
                            if (chunk[px] != transp_idx) frame[pixeloff] =  color;
                            else frame[pixeloff] = prev_frame[pixeloff];
                            break;
                    }

                    pixel_index++;
                }
                //}
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
                    uint8_t gce_block[6];
                    bytesRead = read(fd, &gce_block, 5+1);

                    uint8_t block_size = gce_block[0];
                    if (block_size != 4) RFAILEDP
                    uint8_t packed = gce_block[1];
                    uint16_t delay_time = gce_block[2] | (gce_block[3] << 8);
                    uint8_t transp_index = gce_block[4];
                    uint8_t terminator = gce_block[5];
                    if (terminator != 0) RFAILEDP
                    if (delay_time < 1) delay_time = 10;
                    
                    current_gce.delay_time = delay_time;
                    current_gce.transparent_index = transp_index;
                    current_gce.disposal_method = (packed >> 2) & 0b111;
                    current_gce.transparent_color_flag = packed & 0x1;
                    printf("Frame %u: delay %dms, transp idx %d, disposal %d, transp flag %d\n", crt_frame, delay_time*10, transp_index, current_gce.disposal_method, current_gce.transparent_color_flag);
                    break;
                case COMMENT_EXTENSION:
                    seek_through_blocks(fd);
                    break;
                case PLAIN_TEXT_EXTENSION:
                    lseek(fd, 13, SEEK_CUR); // block size + 12 bytes of data
                    seek_through_blocks(fd);
                    break;
                case APPLICATION_EXTENSION:
                    lseek(fd, 12, SEEK_CUR);
                    seek_through_blocks(fd);
                    break;
                default:
                    // skip other extensions for now
                    while (1) {
                        uint8_t sub_block_size;
                        bytesRead = read(fd, &sub_block_size, 1);
                        if (bytesRead < 1) { free(img); RFAILEDP }
                        if (sub_block_size == 0) break;
                        // skip the data
                        lseek(fd, sub_block_size, SEEK_CUR);
                    }
                    continue;
            }
        } 
    }

    close(fd);
    img->width = width;
    img->height = height;
    return img;
}