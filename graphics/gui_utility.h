#pragma once
#include "graphics.h"
#include <stdint.h>

#define BUTTON_STATE_NORMAL 0
#define BUTTON_STATE_HOVER 1
#define BUTTON_STATE_CLICK 2

#define BUTTON_TIMELINE_PLAYSTOP 0

#define TEXTBOX_TYPE_INT32 0
#define TEXTBOX_TYPE_STRING 1

#define TEXTBOX_STATE_NORMAL 0
#define TEXTBOX_STATE_HOVER 1
#define TEXTBOX_STATE_TYPING 2

struct GUIButton {
    uint8_t weldToWindow;
    uint32_t localX;
    uint32_t localY;
    uint32_t width;
    uint32_t height;

    uint8_t state;

    uint32_t buttonID;
    char* text;
    uint32_t iconID;
    struct GUIButton* nextButton;
};

struct GUITextBox {
    uint8_t type;
    uint32_t numeric_value;
    char* string_value;

    uint32_t localX;
    uint32_t localY;
    uint32_t width;
    uint32_t height;

    uint8_t state;
    uint8_t dirty;

    uint16_t capacity;

    struct GUITextBox* nextTextBox;
};

struct GUIButton* createButton();
void addButton(struct GUIButton* btn);
void drawButton(struct GUIButton* btn);
void buttonCallback(struct GUIButton* btn);
void setButtonState(struct GUIButton* btn, uint8_t state);

struct GUITextBox* createTextBox();
void addTextBox(struct GUITextBox* textbox);
void drawTextBox(struct GUITextBox* textbox);

uint32_t getWindowX(uint8_t type);
uint32_t getWindowY(uint8_t type);
int draw_rect_bound(Rect *rect, Rect *bounds, uint32_t color);
