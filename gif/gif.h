#pragma once

#include "../image32.h"

#define RFAILED { perror("Failed to read file"); fclose(fd); return; }
#define RFAILEDP { perror("Failed to read file (2)"); fclose(fd); return 0; }

#define IMAGE_DESCRIPTOR 0x2C
#define EXTENSION_DESCRIPTOR 0x21
#define GRAPHIC_CONTROL_EXTENSION 0xF9
#define COMMENT_EXTENSION 0xFE
#define PLAIN_TEXT_EXTENSION 0x01
#define APPLICATION_EXTENSION 0xFF

#define DISPOSE_UNSPECIFIED 0
#define DISPOSE_NONE 1
#define DISPOSE_BACKGROUND 2
#define DISPOSE_PREVIOUS 3

struct image32* parse(const char *filename);
