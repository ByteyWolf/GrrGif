#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#define RFAILED { perror("Failed to read file"); close(fd); return; }
#define RFAILEDP { perror("Failed to read file"); close(fd); return 0; }

#define PALETTE_GLOBAL 0
#define PALETTE_LOCAL 1

#define IMAGE_DESCRIPTOR 0x2C
#define GRAPHIC_CONTROL_EXTENSION 0x21

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
void parse(const char *filename)
{
    uint32_t global_palette[256];
    uint32_t local_palette[256];

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
            // TODO
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