#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

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
        palette[i] = (0xFF << 24) | (gct[3*i] << 16) | (gct[3*i + 1] << 8) | (gct[3*i + 2]);
    }

    free(gct);
    return palette;
}

// until we can come up with a proper structure
struct image32* parse(const char *filename)
{
    uint32_t global_palette[256];
    uint32_t local_palette[256];
    struct image32* img = malloc(sizeof(struct image32));

    uint8_t use_palette = 0;
    int crt_delay = 0;
    
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }
    // header
    uint8_t ver[6];
    ssize_t bytesRead = read(fd, ver, sizeof(ver));
    if (bytesRead < 6 || (memcmp(ver, "GIF89a", 6) != 0 && memcmp(ver, "GIF87a", 6) != 0)) RFAILED

    // logical screen descriptor or otherwise lsd (not the other lsd)
    uint8_t lsd[7];
    bytesRead = read(fd, lsd, sizeof(lsd));
    if (bytesRead < 7) RFAILED

    uint16_t width  = lsd[0] | (lsd[1] << 8);
    uint16_t height = lsd[2] | (lsd[3] << 8);

    uint8_t packed = lsd[4];
    uint8_t gct_flag = packed >> 7;
    int color_res = ((packed >> 4) & 0x7) + 1;
    //uint8_t sort_flag = (packed >> 3) & 1;
    int gct_entries = 1 << ((packed & 0x7) + 1);

    uint8_t bg_index = lsd[5];
    uint8_t pixel_aspect = lsd[6];

    uint32_t* canvas = malloc(width * height * sizeof(uint32_t));
    if (gct_flag) process_palette(fd, gct_entries, global_palette);

    // now read until we find the trailer
    uint8_t byte;
    while (1) {
        bytesRead = read(fd, &byte, 1);
        if (bytesRead < 1) RFAILED
        if (byte == 0x3B) {
            break;
        } else if (byte == IMAGE_DESCRIPTOR) {
            uint8_t img_desc[9];
            bytesRead = read(fd, img_desc, sizeof(img_desc));
            if (bytesRead < 9) RFAILED
            uint16_t img_left   = img_desc[0] | (img_desc[1] << 8);
            uint16_t img_top    = img_desc[2] | (img_desc[3] << 8);
            uint16_t img_width  = img_desc[4] | (img_desc[5] << 8);
            uint16_t img_height = img_desc[6] | (img_desc[7] << 8);
            uint8_t img_packed = img_desc[8];
            uint8_t lct_flag = img_packed >> 7;
            uint8_t interlace_flag = (img_packed >> 6) &  1;
            uint8_t sort_flag = (img_packed >> 5) & 1;
            int lct_entries = 1 << ((img_packed & 0x7) + 1);
            if (lct_flag) {
                use_palette = PALETTE_LOCAL;
                process_palette(fd, lct_entries, local_palette);
            }

            // is rendering time!!
            uint32_t* frame = malloc(img_width * img_height * sizeof(uint32_t));
            uint8_t lzw_min_code_size;
            bytesRead = read(fd, &lzw_min_code_size, 1);
            if (bytesRead < 1) RFAILEDP

            if (!frame) RFAILEDP

            lzw_decoder_t decoder;
            LZWInit(&decoder, lzw_min_code_size);
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

                for (int i = 0; i < sub_block_size; i++) {
                    int outSize;
                    const unsigned char* chunk = LZWFeedCode(&decoder, sub_block_data[i], &outSize);

                    // iterate through decoded bytes and write them into canvas or something
                }
            }
        } else if (byte == GRAPHIC_CONTROL_EXTENSION) {
            uint8_t gce_block[7];
            bytesRead = read(fd, &gce_block, 1+5+1);

            if (bytesRead < 1 || gce_block[0] != 0xF9) RFAILED
            uint8_t block_size = gce_block[1];
            if (block_size != 4) RFAILED
            uint8_t packed = gce_block[2];
            uint16_t delay_time = gce_block[3] | (gce_block[4] << 8);
            uint8_t transp_index = gce_block[5];
            uint8_t terminator = gce_block[6];
            if (terminator != 0) RFAILED
            // TODO
        }
            
    }

    close(fd);
}