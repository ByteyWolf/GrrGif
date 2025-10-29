#!/usr/bin/env python3
from PIL import Image

GLYPH_W, GLYPH_H = 6, 12
GRID_W, GRID_H = 16, 8
NEW_W = 8
ASCII_COUNT = 128

# load BMP as monochrome
im = Image.open("font.bmp").convert("1")  # 1-bit
pixels = im.load()

out_lines = ["#include <stdint.h>", "static const uint8_t font8x12[128][12] = {"]

for g in range(ASCII_COUNT):
    gx, gy = g % GRID_W, g // GRID_W
    out_lines.append("  {")
    for y in range(GLYPH_H):
        byte = 0
        for x in range(GLYPH_W):
            px = gx * GLYPH_W + x
            py = gy * GLYPH_H + y
            if pixels[px, py] == 0:
                byte |= (1 << x)  # left-align
        out_lines.append(f"0x{byte:02X}," if y < GLYPH_H - 1 else f"0x{byte:02X}")
    out_lines.append("}," if g < ASCII_COUNT - 1 else "}")
out_lines.append("};\n")

with open("font8x12.h", "w") as f:
    f.write("\n".join(out_lines))

print("font8x12.h generated!")
