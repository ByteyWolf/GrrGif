#include "../image32.h"

#define RFAILED { perror("Failed to read file"); close(fd); return; }
#define RFAILEDP { perror("Failed to read file"); close(fd); return 0; }

#define PALETTE_GLOBAL 0
#define PALETTE_LOCAL 1

#define IMAGE_DESCRIPTOR 0x2C
#define GRAPHIC_CONTROL_EXTENSION 0x21

struct image32* parse(const char *filename);
