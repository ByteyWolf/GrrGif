#pragma once

#define FILE_ANIMATION 0
#define FILE_STILLFRAME 1
#define FILE_TEXT 2

#define FONT_NORMAL 0
#define FONT_BOLD 1
#define FONT_ITALIC 2
#define FONT_UNDERLINE 4

#define MAX_TRACKS 9

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

    uint32_t refCount;
};

struct Effect {
    uint32_t effectId;
    char* params;
    uint32_t paramsLen;
    struct Effect* nextEffect;
};

struct TimelineObject {
    int x;
    int y;
    uint32_t width;
    uint32_t height;
    uint32_t timePosMs;
    uint32_t length;

    uint32_t fakeTimePosMs;
    uint8_t fakeTrack;
    uint8_t beingDragged;

    uint8_t track;

    char* fileName;
    struct LoadedFile* metadata;
    struct Effect* effectsList;

    struct TimelineObject* nextObject;
};

struct Timeline {
    struct TimelineObject* first;
    struct TimelineObject* last;
    uint32_t length;
    uint32_t objects;
};

struct LoadedFile* findLoadedFile(char* filepath);
int insertTimelineObj(struct TimelineObject* obj, uint8_t track);
void insertTimelineObjFree(struct TimelineObject* obj);
void destroyTimelineObj(struct TimelineObject* obj);
void timeline_heartbeat();
uint64_t current_time_ms();
