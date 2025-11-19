#include "../modules.h"
#include "../graphics/graphics.h"
#include "../graphics/gui_utility.h"
#include "../timeline/timeline.h"
#include "../debug.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TRACK_HEIGHT_PX 40
#define TOPBAR_HEIGHT_PX 30

static struct GUIButton* play;
static uint32_t scroll_y = 0;
static uint32_t x_bound_l = 0;
static uint32_t x_bound_r = 2000;

extern uint32_t crtTimelineMs;
extern uint32_t timelineLengthMs;
extern struct Timeline tracks[];
extern struct ModuleWindow windows[3];
extern struct TimelineObject* selectedObj;
extern uint8_t mouseDown;

static uint32_t initialMsMouse = 0;
static uint32_t dragStartMs = 0;
static uint8_t dragStartTrack = 0;

struct bound_data {
    uint32_t bound_count;
    uint32_t* bounds;
};

static struct bound_data obj_bounds[MAX_TRACKS] = {0};

uint32_t doInvert(uint32_t color, uint8_t invert) {
    if (invert == 2) return 0xFFFFFF;
    if (invert) return 0xFFFFFF - color;
    return color;
}

uint8_t check_overlap(struct TimelineObject* obj, uint32_t pos, uint32_t length, uint8_t track) {
    struct TimelineObject* crtObj = tracks[track].first;
    uint32_t obj_right = pos + length;

    while (crtObj) {
        if (crtObj == obj) {
            crtObj = crtObj->nextObject;
            continue;
        }

        uint32_t crt_right = crtObj->timePosMs + crtObj->length;

        if (!(obj_right <= crtObj->timePosMs || pos >= crt_right)) {
            return 1;
        }

        crtObj = crtObj->nextObject;
    }
    return 0;
}

// todo: Fix
void calculate_bounds() {
    for (uint8_t track = 0; track < MAX_TRACKS - 1; track++) {
        uint32_t* bounds = obj_bounds[track].bounds;
        if (bounds) free(bounds);
        obj_bounds[track].bounds = malloc(sizeof(uint32_t) * tracks[track].objects * 2);
        obj_bounds[track].bound_count = tracks[track].objects;
        struct TimelineObject* crtObj = tracks[track].first;
        uint32_t tracker = 0;
        while (crtObj) {
            if (tracker >= tracks[track].objects * 2) break;
            obj_bounds[track].bounds[tracker] = crtObj->timePosMs;
            tracker++;
            obj_bounds[track].bounds[tracker] = crtObj->timePosMs + crtObj->length;
            tracker++;
            crtObj = crtObj->nextObject;
        }
    }
    
}

void remove_from_track(struct TimelineObject* obj) {
    struct TimelineObject* crtObj = tracks[obj->track].first;
    struct TimelineObject* prevObj = NULL;

    while (crtObj) {
        if (crtObj == obj) {
            if (prevObj) {
                prevObj->nextObject = obj->nextObject;
            } else {
                tracks[obj->track].first = obj->nextObject;
            }
            if (!obj->nextObject) {
                tracks[obj->track].last = prevObj;
            }
            return;
        }
        prevObj = crtObj;
        crtObj = crtObj->nextObject;
    }
    tracks[obj->track].objects --;
}

void insert_into_track(struct TimelineObject* obj, uint8_t track) {
    obj->track = track;
    obj->nextObject = NULL;

    if (!tracks[track].first) {
        tracks[track].first = obj;
        tracks[track].last = obj;
        return;
    }

    struct TimelineObject* crtObj = tracks[track].first;
    struct TimelineObject* prevObj = NULL;

    while (crtObj) {
        if (crtObj->timePosMs > obj->timePosMs) {
            obj->nextObject = crtObj;
            if (prevObj) {
                prevObj->nextObject = obj;
            } else {
                tracks[track].first = obj;
            }
            return;
        }
        prevObj = crtObj;
        crtObj = crtObj->nextObject;
    }

    prevObj->nextObject = obj;
    tracks[track].last = obj;
}

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

    for (uint8_t track = 0; track < MAX_TRACKS - 1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        if (track < trackIdMin || track >= trackIdMax) continue;

        while (crtObj) {
            if (!crtObj->beingDragged) {
                crtObj->fakeTimePosMs = crtObj->timePosMs;
                crtObj->fakeTrack = crtObj->track;
            }

            if (crtObj->fakeTimePosMs + crtObj->length < x_bound_l || crtObj->fakeTimePosMs > x_bound_r) {
                crtObj = crtObj->nextObject;
                continue;
            }

            uint32_t y_offset = y + 71 - (scroll_y % TRACK_HEIGHT_PX) + TRACK_HEIGHT_PX * (crtObj->fakeTrack - trackIdMin);

            uint32_t x_left = crtObj->fakeTimePosMs < x_bound_l ? x_bound_l : crtObj->fakeTimePosMs;
            uint32_t x_right = crtObj->fakeTimePosMs + crtObj->length > x_bound_r ? x_bound_r : crtObj->fakeTimePosMs + crtObj->length;

            x_left = (x_left - x_bound_l) * (width - 40) / (x_bound_r - x_bound_l);
            x_right = (x_right - x_bound_l) * (width - 40) / (x_bound_r - x_bound_l);

            tmprect.x = x + 40 + x_left;
            tmprect.y = y_offset;
            tmprect.width = x_right - x_left;
            tmprect.height = TRACK_HEIGHT_PX - 2;

            uint8_t modifier = crtObj == selectedObj;
            if (crtObj->beingDragged) modifier = 2;

            draw_rect_bound(&tmprect, &windowBounds, doInvert(0x004477, modifier));
            shrink_rect(&tmprect, 1);
            draw_rect_bound(&tmprect, &windowBounds, doInvert(0x006699, modifier));
            shrink_rect(&tmprect, 1);
            draw_rect_bound(&tmprect, &windowBounds, doInvert(0x0088BB, modifier));
            shrink_rect(&tmprect, 3);

            draw_text_limited(crtObj->fileName, tmprect.x, tmprect.y, modifier > 1 ? 0 : 0xFFFFFF, ANCHOR_LEFT, tmprect.width);

            crtObj = crtObj->nextObject;
        }
    }

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

uint8_t get_mouse_data(Event* event, uint32_t* timelineMsClick, uint8_t* track) {
    if (event->x < 40) return 0;

    uint32_t mouseX = event->x - 40;
    uint32_t percentage = mouseX * 100 / (windows[POSITION_BOTTOM].width - 40);
    *timelineMsClick = x_bound_l + ((x_bound_r - x_bound_l) * percentage / 100);

    int mouseY = event->y + scroll_y - 70 - windows[POSITION_TOPLEFT].height;
    if (mouseY < 0) return 0;

    *track = mouseY / TRACK_HEIGHT_PX;
    if (*track >= MAX_TRACKS) return 0;

    return 1;
}

uint8_t get_bounds_data(uint32_t timelineMsClick, uint8_t track, uint8_t* bound_side, uint32_t* hit_id) {
    if (obj_bounds[track].bounds && obj_bounds[track].bound_count > 0) {
        printf("bounds for track %u: ", track);
        for (uint32_t boundId = 0; boundId < (obj_bounds[track].bound_count)*2; boundId++) {
            
            int diff = obj_bounds[track].bounds[boundId] - timelineMsClick;
            printf("%u ", obj_bounds[track].bounds[boundId]);
            if (diff > -40 && diff < 40) {
                *bound_side = boundId % 2;
                *hit_id = boundId / 2;
                return 1;
            }
        }
        putchar('\n');
    }
    return 0;
}

// todo: show resize cursor by defining uint32_t bounds[]

void timeline_handle_event(Event* event) {
    if (event->x <= 40 || event->y <= windows[POSITION_TOPLEFT].height) return;

    calculate_bounds();

    switch (event->type) {
        case EVENT_MOUSEBUTTONDOWN: {
            selectedObj = NULL;
            uint8_t trackClick;
            uint32_t timelineMsClick;
            if (!get_mouse_data(event, &timelineMsClick, &trackClick)) break;

            debugf(DEBUG_VERBOSE, "Clicked on %u ms, track %u.", timelineMsClick, trackClick);

            struct TimelineObject* crtItem = tracks[trackClick].first;
            while (crtItem) {
                if (crtItem->timePosMs <= timelineMsClick && crtItem->timePosMs + crtItem->length > timelineMsClick) {
                    selectedObj = crtItem;
                    dragStartMs = crtItem->timePosMs;
                    dragStartTrack = crtItem->track;
                    initialMsMouse = timelineMsClick;

                    if (crtItem->timePosMs > crtTimelineMs) {
                        crtTimelineMs = crtItem->timePosMs + 1;
                    } else if (crtItem->timePosMs + crtItem->length < crtTimelineMs) {
                        crtTimelineMs = crtItem->timePosMs + crtItem->length;
                    }

                    debugf(DEBUG_VERBOSE, "Selected file %s.", crtItem->fileName);
                    break;
                }
                crtItem = crtItem->nextObject;
            }

            if (!selectedObj) {
                crtTimelineMs = timelineMsClick;
            }
            break;
        }

        case EVENT_MOUSEMOVE: {
            uint8_t trackClick;
            uint32_t timelineMsClick;
            if (!get_mouse_data(event, &timelineMsClick, &trackClick)) break;

            uint8_t bound;
            uint32_t boundId;
            if (get_bounds_data(timelineMsClick, trackClick, &bound, &boundId)) {
               set_cursor(CURSOR_SIZEH); 
            } else set_cursor(CURSOR_NORMAL);
            
            
            if (!selectedObj || !mouseDown) break;

            int msDelta = (int)timelineMsClick - (int)initialMsMouse;
            int32_t newPos = (int32_t)dragStartMs + msDelta;
            if (newPos < 0) newPos = 0;

            selectedObj->fakeTimePosMs = (uint32_t)newPos;
            selectedObj->fakeTrack = trackClick;
            selectedObj->beingDragged = 1;
            break;
        }

        case EVENT_MOUSEBUTTONUP: {
            if (!selectedObj || !selectedObj->beingDragged) break;

            uint32_t targetPos = selectedObj->fakeTimePosMs;
            uint8_t targetTrack = selectedObj->fakeTrack;

            if (!check_overlap(selectedObj, targetPos, selectedObj->length, targetTrack)) {
                if (targetTrack != selectedObj->track) {
                    remove_from_track(selectedObj);
                    selectedObj->timePosMs = targetPos;
                    insert_into_track(selectedObj, targetTrack);
                } else {
                    selectedObj->timePosMs = targetPos;
                }

                uint32_t rbound = selectedObj->timePosMs + selectedObj->length;
                if (rbound > timelineLengthMs) timelineLengthMs = rbound;
                if (rbound > tracks[selectedObj->track].length) tracks[selectedObj->track].length = rbound;
            } else {
                selectedObj->fakeTimePosMs = selectedObj->timePosMs;
                selectedObj->fakeTrack = selectedObj->track;
            }

            selectedObj->beingDragged = 0;
            break;
        }
    }
}
