#pragma once
#include <stdint.h>

#define ANCHOR_N 1
#define ANCHOR_S 2
#define ANCHOR_W 4
#define ANCHOR_E 8

struct GUIButton {
    uint8_t weldToWindow;
    uint32_t localX;
    uint32_t localY;
    uint32_t width;
    uint32_t height;

    uint32_t buttonID;
    char* text;
    uint32_t iconID;

    struct GUIButton* nextButton;
};

struct GUIButton* createButton();
void addButton(struct GUIButton* btn);
void drawButton(struct GUIButton* btn);