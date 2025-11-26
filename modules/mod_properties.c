#include "../modules.h"
#include "../graphics/graphics.h"
#include "../graphics/gui_utility.h"
#include "../timeline/timeline.h"
#include "../debug.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct Timeline tracks[];
extern struct ModuleWindow windows[3];
extern struct TimelineObject* selectedObj;

extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

static char scratch_space[256] = {0};
static Rect rect;
static uint32_t rect_color = 0x333333;

static uint8_t boxFileWidth = 0xFF;
static uint8_t boxFileHeight = 0xFF;
static uint8_t boxObjWidth = 0xFF;
static uint8_t boxObjHeight = 0xFF;

void reset_rect(uint32_t x, uint32_t width) {
    rect_color = 0x333333;
    rect.width = width - 16;
    rect.x = x + 8;
    rect.height = 20;
    rect.y = 69;
}

void step_rect() {
    draw_rect(&rect, rect_color);
    rect_color = (rect_color == 0x333333) ? 0x222222 : 0x333333;
    rect.y += rect.height;
}

void draw_file(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    setEditBoxVisible(boxObjWidth, 0);
    setEditBoxVisible(boxObjHeight, 0);
    
    draw_text("This project", x + 10, y + 40, 0xFFFFFF);
    draw_text("This project", x + 11, y + 40, 0xFFFFFF);

    reset_rect(x, width);
    
    step_rect();
    
    draw_text_anchor("Width", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", fileWidthPx);
    if (!boxIsFocused(boxFileWidth))    setEditBoxText(boxFileWidth, scratch_space);
    moveEditBox(boxFileWidth, x + width - 100, rect.y - 18, 90, 18);
    
    step_rect();
    
    draw_text_anchor("Height", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", fileHeightPx);
    if (!boxIsFocused(boxFileHeight))    setEditBoxText(boxFileHeight, scratch_space);
    moveEditBox(boxFileHeight, x + width - 100, rect.y - 18, 90, 18);

    rect.y += 40;
    draw_text_anchor("Export", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    draw_text_anchor("Export", x + 11, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    step_rect();
     
    draw_text_anchor("Quality", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    draw_text_anchor("Fastest", x + width - 10, rect.y - 19, 0xCCCCCC, ANCHOR_RIGHT);

    setEditBoxVisible(boxFileWidth, 1);
    setEditBoxVisible(boxFileHeight, 1);

    // update stuff
    getEditBoxText(boxFileWidth, scratch_space, sizeof(scratch_space));
    int val = atoi(scratch_space);
    if (val > 0) fileWidthPx = val;

    getEditBoxText(boxFileHeight, scratch_space, sizeof(scratch_space));
    val = atoi(scratch_space);
    if (val > 0) fileHeightPx = val;
}

void draw_object(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    setEditBoxVisible(boxFileWidth, 0);
    setEditBoxVisible(boxFileHeight, 0);
    
    draw_text_limited(selectedObj->fileName, x + 10, y + 40, 0xFFFFFF, ANCHOR_LEFT, width - 20);
    draw_text_limited(selectedObj->fileName, x + 11, y + 40, 0xFFFFFF, ANCHOR_LEFT, width - 20);

    reset_rect(x, width);
    
    step_rect();
    
    draw_text_anchor("Width", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", selectedObj->width);
    if (!boxIsFocused(boxObjWidth))    setEditBoxText(boxObjWidth, scratch_space);
    moveEditBox(boxObjWidth, x + width - 100, rect.y - 18, 90, 18);
    
    step_rect();
    
    draw_text_anchor("Height", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", selectedObj->height);
    if (!boxIsFocused(boxObjHeight))    setEditBoxText(boxObjHeight, scratch_space);
    moveEditBox(boxObjHeight, x + width - 100, rect.y - 18, 90, 18);

    step_rect();
    
    draw_text_anchor("X", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%d", selectedObj->x);
    draw_text_anchor(scratch_space, x + width - 10, rect.y - 19, 0xCCCCCC, ANCHOR_RIGHT);
    
    step_rect();
    
    draw_text_anchor("Y", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%d", selectedObj->y);
    draw_text_anchor(scratch_space, x + width - 10, rect.y - 19, 0xCCCCCC, ANCHOR_RIGHT);

    rect.y += 40;
    draw_text_anchor("Timeline", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    draw_text_anchor("Timeline", x + 11, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    step_rect();
     
    draw_text_anchor("Timestamp", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", selectedObj->timePosMs);
    draw_text_anchor(scratch_space, x + width - 10, rect.y - 19, 0xCCCCCC, ANCHOR_RIGHT);
    
    step_rect();
     
    draw_text_anchor("Duration", x + 10, rect.y - 19, 0xCCCCCC, ANCHOR_LEFT);
    snprintf(scratch_space, sizeof(scratch_space), "%u", selectedObj->length);
    draw_text_anchor(scratch_space, x + width - 10, rect.y - 19, 0xCCCCCC, ANCHOR_RIGHT);

    setEditBoxVisible(boxObjWidth, 1);
    setEditBoxVisible(boxObjHeight, 1);

    // update stuff
    getEditBoxText(boxObjWidth, scratch_space, sizeof(scratch_space));
    int val = atoi(scratch_space);
    if (val > 0) selectedObj->width = val;

    getEditBoxText(boxObjHeight, scratch_space, sizeof(scratch_space));
    val = atoi(scratch_space);
    if (val > 0) selectedObj->height = val;
}

void properties_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (width < 50 || width > 0xFFFF || height < 50 || height > 0xFFFFF) return;
    if (boxFileWidth == 0xFF) {boxFileWidth = createEditBox(0x333333);}
    if (boxFileHeight == 0xFF) {boxFileHeight = createEditBox(0x222222);}
    
    if (boxObjWidth == 0xFF) {boxObjWidth = createEditBox(0x333333);}
    if (boxObjHeight== 0xFF) {boxObjHeight = createEditBox(0x222222);} 
    
    set_font_size(FONT_SIZE_NORMAL);
    if (!selectedObj) draw_file(x, y, width, height); else draw_object(x, y, width, height);
}
