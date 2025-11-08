#pragma once

#define FILE_ANIMATION 0
#define FILE_STILLFRAME 1
#define FILE_TEXT 2

#define FONT_NORMAL 0
#define FONT_BOLD 1
#define FONT_ITALIC 2
#define FONT_UNDERLINE 4

#include <stdint.h>

struct LoadedFile {
    uint8_t type;

    // applicable if type is 0 or 1
    struct imageV2* imagePtr;

    // applicable if type is 2
    char* text;
    char* textFont;
    uint32_t fontSize;
    uint8_t fontStyle;
};

struct Effect {
    uint32_t effectId;
    char* params;
    uint32_t paramsLen;
    struct Effect* nextEffect;
};

struct TimelineObject {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t timePosMs;
    uint32_t length;

    uint32_t track;

    char* fileName;
    struct LoadedFile* metadata;
    struct Effect* effectsList;

    struct TimelineObject* nextObject;
};

int insertTimelineObj(struct TimelineObject* obj);
void timeline_heartbeat();
uint64_t current_time_ms();
