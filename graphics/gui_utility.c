#include <stdint.h>
#include <stdlib.h>
#include "../modules.h"
#include "gui_utility.h"
#include "graphics.h"

struct GUIButton* buttonBase = 0;
extern struct ModuleWindow windows[3];
extern uint32_t wwidth;
extern uint32_t wheight;

extern uint8_t previewPlaying;

struct GUIButton* createButton() {
    struct GUIButton* btn = malloc(sizeof(struct GUIButton));
    return btn;
}

uint32_t getWindowX(uint8_t type) {
    switch (type) {
        case POSITION_BOTTOM:
        case POSITION_TOPLEFT:
            return 0;
        case POSITION_TOPRIGHT:
            return wwidth - windows[type].width;
    }
    return 0;
}

uint32_t getWindowY(uint8_t type) {
    switch (type) {
        case POSITION_TOPRIGHT:
        case POSITION_TOPLEFT:
            return 0;
        case POSITION_BOTTOM:
            return wheight - windows[type].height;
    }
    return 0;
}

void addButton(struct GUIButton* btn) {
    btn->nextButton = 0;
    if (!buttonBase) {buttonBase = btn; return;}
    buttonBase->nextButton = btn;
}

void drawButton(struct GUIButton* btn, uint8_t state) {
    Rect btnRect;
    btnRect.x = btn->localX + getWindowX(btn->weldToWindow);
    btnRect.y = btn->localY + getWindowY(btn->weldToWindow);
    btnRect.height = btn->height;
    btnRect.width = btn->width;

    uint32_t fillclr;
    uint32_t textclr;
    uint32_t borderclr;
    switch (state) {
        case BUTTON_STATE_NORMAL:
            borderclr = 0xAAAAAA;
            fillclr = 0x303030;
            textclr = 0xFFFFFF;
            break;
        case BUTTON_STATE_HOVER:
            borderclr = 0xAAAAAA;
            fillclr = 0xAAAAAA;
            textclr = 0x000000;
            break;
        case BUTTON_STATE_CLICK:
            borderclr = 0xFFFFFF;
            fillclr = 0xFFFFFF;
            textclr = 0x000000;
            break;
        default:
            return; 
    }

    draw_rect(&btnRect, borderclr);
    shrink_rect(&btnRect, 1);
    draw_rect(&btnRect, 0xFFFFFF);
    shrink_rect(&btnRect, 1);
    draw_rect(&btnRect, fillclr);
    if (btn->text) {
        set_font_size(8);
        draw_text(btn->text, btnRect.x + 2, btnRect.y, textclr);
    }
}

void buttonCallback(struct GUIButton* btn) {
    switch (btn->buttonID) {
        case BUTTON_TIMELINE_PLAYSTOP: {
            previewPlaying = !previewPlaying;
            btn->text = previewPlaying ? "Stop" : "Play";
        }
    }
}