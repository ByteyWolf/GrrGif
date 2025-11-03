#include <stdint.h>
#include <stdlib.h>
#include "../modules.h"
#include "gui_utility.h"
#include "graphics.h"

struct GUIButton* buttonBase = 0;
extern struct ModuleWindow windows[3];
extern uint32_t wwidth;
extern uint32_t wheight;

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

void drawButton(struct GUIButton* btn) {
    Rect btnRect;
    btnRect.x = btn->localX + getWindowX(btn->weldToWindow);
    btnRect.y = btn->localY + getWindowY(btn->weldToWindow);
    btnRect.height = btn->height;
    btnRect.width = btn->width;

    draw_rect(&btnRect, 0xAAAAAA);
    shrink_rect(&btnRect, 1);
    draw_rect(&btnRect, 0xFFFFFF);
    shrink_rect(&btnRect, 1);
    draw_rect(&btnRect, 0x303030);
    if (btn->text) {
        set_font_size(8);
        draw_text(btn->text, btnRect.x + 2, btnRect.y, 0xFFFFFF);
    }
}