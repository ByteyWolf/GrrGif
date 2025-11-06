#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "../image32.h"
#include "timeline.h"

uint32_t timelineObjects;
uint32_t timelineMaxMs;
struct TimelineObject** sequentialAccess = 0;
struct TimelineObject* timeline = 0;

uint32_t crtTimelineMs = 0;
uint32_t timelineLengthMs = 0;
uint8_t previewPlaying = 0;
uint32_t fileWidthPx = 1200;
uint32_t fileHeightPx = 600;

uint64_t lastTimelineEpoch = 0;

extern uint8_t pendingRedraw;

#if defined(_WIN32)
#include <windows.h>

uint64_t current_time_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    // Convert FILETIME (100-ns intervals since 1601) to milliseconds since epoch
    uint64_t t = (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000;
    t -= 11644473600000ULL; // Windows FILETIME epoch → Unix epoch
    return t;
}

#else
#include <sys/time.h>

uint64_t current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif


int insertTimelineObj(struct TimelineObject* obj) {
    if (!obj) return 1;

    struct TimelineObject** newSeq = realloc(sequentialAccess, (timelineObjects+1) * sizeof(*sequentialAccess));
    if (!newSeq) return 1;
    sequentialAccess = newSeq;

    uint32_t i = 0;
    while (i < timelineObjects && sequentialAccess[i]->timePosMs <= obj->timePosMs) i++;

    for (uint32_t j = timelineObjects; j > i; j--) {
        sequentialAccess[j] = sequentialAccess[j-1];
    }

    sequentialAccess[i] = obj;

    if (i > 0)
        sequentialAccess[i-1]->nextObject = obj;
    obj->nextObject = (i < timelineObjects) ? sequentialAccess[i+1] : NULL;

    timelineObjects++;
    if (i==0) timeline = obj;
    if (obj->timePosMs + obj->length > timelineLengthMs) timelineLengthMs = obj->timePosMs + obj->length;
    printf("new timeline length %u\n", timelineLengthMs);

    return 0;
}

void timeline_heartbeat() {
    if (!lastTimelineEpoch) lastTimelineEpoch = current_time_ms();
    if (previewPlaying && timelineLengthMs > 0) {
        crtTimelineMs += (current_time_ms() - lastTimelineEpoch);
        crtTimelineMs %= timelineLengthMs;
        lastTimelineEpoch = current_time_ms();
    }
}
