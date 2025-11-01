#pragma once

#define FILE_ANIMATION 0
#define FILE_STILLFRAME 1
#define FILE_TEXT 2

#define FONT_NORMAL 0
#define FONT_BOLD 1
#define FONT_ITALIC 2
#define FONT_UNDERLINE 4

struct LoadedFile {
    uint8_t type;

    // applicable if type is 0 or 1
    struct image32* imagePtr;

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

    char* fileSource;
    struct LoadedFile* metadata;
    struct Effect* effectsList;

    struct TimelineObject* nextObject;
};

int insertTimelineObj(struct TimelineObject* obj);