#include "../modules.h"
#include "../graphics/graphics.h"
#include "../graphics/gui_utility.h"
#include "../timeline/timeline.h"
#include <stdint.h>
#include <stdio.h>

#define TRACK_HEIGHT_PX 40
#define TOPBAR_HEIGHT_PX 30

static struct GUIButton* play;
static uint32_t scroll_y = 0;
static uint32_t x_bound_l = 0;
static uint32_t x_bound_r = 2000;

extern uint32_t crtTimelineMs;
extern uint32_t timelineLengthMs;
extern struct Timeline tracks[];

void timeline_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    Rect windowBounds;
    windowBounds.x = x + 7;
    windowBounds.y = y + 7;
    windowBounds.width = width - 14;
    windowBounds.height = height - 14;

    uint8_t trackIdMin = scroll_y / TRACK_HEIGHT_PX;
    uint8_t trackIdMax = (scroll_y + height) / TRACK_HEIGHT_PX + 1;
    if (trackIdMax >= MAX_TRACKS) trackIdMax = MAX_TRACKS - 1;

    Rect tmprect;
    set_font_size(8);

    // draw tracks
    for (uint8_t trackId = trackIdMin; trackId < trackIdMax; trackId++) {
        uint32_t y_offset = y + 70 - (scroll_y % TRACK_HEIGHT_PX) + TRACK_HEIGHT_PX * (uint32_t)(trackId - trackIdMin);
        if (y_offset < y + 30) continue;
        if (y_offset > y + height - 30) break;

        tmprect.x = x + 40;
        tmprect.y = y_offset;
        tmprect.width = width - 40;
        tmprect.height = TRACK_HEIGHT_PX;
        draw_rect_bound(&tmprect, &windowBounds, 0x505050);
        shrink_rect(&tmprect, 1);
        draw_rect_bound(&tmprect, &windowBounds, 0x404040);
        shrink_rect(&tmprect, 1);
        draw_rect_bound(&tmprect, &windowBounds, 0x202020);

        char tmp[20];
        snprintf(tmp, sizeof(tmp), "%u", trackId + 1);
        draw_text(tmp, x + 10, y_offset, 0xFFFFFF);
    }

    // draw timeline objects
    for (uint8_t track = 0; track<MAX_TRACKS-1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        if (track < trackIdMin || track >= trackIdMax) { continue; }
    
        while (crtObj) {
            if (crtObj->timePosMs + crtObj->length < x_bound_l || crtObj->timePosMs > x_bound_r) { crtObj = crtObj->nextObject; continue; }
    
            uint32_t y_offset = y + 71 - (scroll_y % TRACK_HEIGHT_PX) + TRACK_HEIGHT_PX * (track - trackIdMin);
    
            uint32_t x_left = crtObj->timePosMs < x_bound_l ? x_bound_l : crtObj->timePosMs;
            uint32_t x_right = crtObj->timePosMs + crtObj->length > x_bound_r ? x_bound_r : crtObj->timePosMs + crtObj->length;
    
            x_left = (x_left - x_bound_l) * (width - 40) / (x_bound_r - x_bound_l);
            x_right = (x_right - x_bound_l) * (width - 40) / (x_bound_r - x_bound_l);
    
            tmprect.x = x + 40 + x_left;
            tmprect.y = y_offset;
            tmprect.width = x_right - x_left;
            tmprect.height = TRACK_HEIGHT_PX - 2;
            draw_rect_bound(&tmprect, &windowBounds, 0x004477);
            shrink_rect(&tmprect, 1);
            draw_rect_bound(&tmprect, &windowBounds, 0x006699);
            shrink_rect(&tmprect, 1);
            draw_rect_bound(&tmprect, &windowBounds, 0x0088BB);
            shrink_rect(&tmprect, 3);
    
            draw_text_limited(crtObj->fileName, tmprect.x, tmprect.y, 0xFFFFFF, ANCHOR_LEFT, tmprect.width);
    
            crtObj = crtObj->nextObject;
        }
    }
    

    // draw playhead
    if (crtTimelineMs >= x_bound_l && crtTimelineMs <= x_bound_r) {
        uint32_t playhead_x = x + 40 + (crtTimelineMs - x_bound_l) * (width - 40) / (x_bound_r - x_bound_l);
        tmprect.x = playhead_x;
        tmprect.y = y + 40;
        tmprect.width = 2;
        tmprect.height = height - 60;
        draw_rect_bound(&tmprect, &windowBounds, 0xFF0000);
    }

    tmprect.x = x;
    tmprect.y = y + 30;
    tmprect.width = width;
    tmprect.height = TOPBAR_HEIGHT_PX;
    draw_rect_bound(&tmprect, &windowBounds, 0x303030);

    if (!play) {
        play = createButton();
        play->weldToWindow = POSITION_BOTTOM;
        play->localX = 20;
        play->localY = 35;
        play->width = 40;
        play->height = 20;
        play->text = "Play";
        play->buttonID = BUTTON_TIMELINE_PLAYSTOP;
        addButton(play);
    }

    tmprect.x = x;
    tmprect.y = y + height - 30;
    tmprect.width = width;
    tmprect.height = TOPBAR_HEIGHT_PX;
    draw_rect_bound(&tmprect, &windowBounds, 0x303030);

    char stats[100];
    for (uint32_t xoff = x + 40; xoff < x + width; xoff += 100) {
        uint32_t percentage = (xoff - x - 40) * 100 / (width - 40);
        uint32_t timecodems = x_bound_l + percentage * (x_bound_r - x_bound_l) / 100;
        snprintf(stats, sizeof(stats), "%u.%03u", timecodems / 1000, timecodems % 1000);
        draw_text_anchor(stats, xoff, y + height - 25, 0xFFFFFF, ANCHOR_MIDDLE);
    }

    snprintf(stats, sizeof(stats), "%us %ums / %us %ums", crtTimelineMs / 1000, crtTimelineMs % 1000, timelineLengthMs / 1000, timelineLengthMs % 1000);
    draw_text_anchor(stats, x + width - 18, y + 11, 0xFFFFFF, ANCHOR_RIGHT);
}

void timeline_scroll(int delta) {
    int new_scroll = (int)scroll_y - delta / 20;
    scroll_y = new_scroll >= 0 ? new_scroll : 0;
}
