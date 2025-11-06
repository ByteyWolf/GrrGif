#include "modfunc.h"
#include "../modules.h"
#include "../graphics/graphics.h"
#include "../graphics/gui_utility.h"
#include <stdint.h>

#define TRACK_HEIGHT_PX 40
#define TOPBAR_HEIGHT_PX 30

static struct GUIButton* play;
static uint32_t scroll_y;
static uint32_t x_bound_l;
static uint32_t x_bound_r;

void timeline_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    // draw the tracks
    Rect windowBounds = {0};
    windowBounds.x = x + 7;
    windowBounds.y = y + 7;
    windowBounds.width = width-14;
    windowBounds.height = height-14;
    
    uint32_t trackIdMin = scroll_y / TRACK_HEIGHT_PX;
    uint32_t trackIdMax = (scroll_y + height) / TRACK_HEIGHT_PX;

    Rect tmprect = {0};
    tmprect.x = x;
    
    for (uint32_t trackId=trackIdMin; trackId < trackIdMax; trackId++) {
        uint32_t y_offset = y + 50 + (scroll_y % TRACK_HEIGHT_PX);
        
        tmprect.y = y_offset;
        tmprect.width = width;
        tmprect.height = TRACK_HEIGHT_PX;
        draw_rect_bound(&tmprect, &windowBounds, 0x505050);
        shrink_rect(&tmprect, 1);
        draw_rect_bound(&tmprect, &windowBounds, 0x404040);
        shrink_rect(&tmprect, 1);
        draw_rect_bound(&tmprect, &windowBounds, 0x202020);
    }
    
    // draw the topbar
    tmprect.x = x;
    tmprect.y = y + 30;
    tmprect.width = width;
    tmprect.height = TOPBAR_HEIGHT_PX;
    draw_rect_bound(&tmprect, &windowBounds, 0x303030);
    if (!play) {play = createButton(); 
        play->weldToWindow = POSITION_BOTTOM; 
        play->localX = 20;
        play->localY = 35;
        play->width = 40;
        play->height = 20;
        play->text = "Play";
        play->buttonID = BUTTON_TIMELINE_PLAYSTOP;
        addButton(play);
    }
    
}


