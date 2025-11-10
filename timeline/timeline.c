#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "../image32.h"
#include "../debug.h"
#include "timeline.h"
#include "../graphics/graphics.h"

uint32_t timelineObjects;
uint32_t timelineMaxMs;
struct Timeline tracks[MAX_TRACKS] = {0};

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
    t -= 11644473600000ULL; // Windows FILETIME epoch to Unix epoch
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


// insert new object at any free spot where the cursor is hovering
int insertTimelineObj(struct TimelineObject* obj, uint8_t track) {
    if (track > MAX_TRACKS - 1) return 1;
    if (!obj) return 1;
    
    uint32_t insert_timestamp = crtTimelineMs;
    struct TimelineObject* insertAfter = 0;
    struct TimelineObject* crtCandidate = tracks[track].first;
    while (crtCandidate) {
        if (crtCandidate->timePosMs >= crtTimelineMs) {
            crtCandidate->timePosMs += obj->length;
            uint32_t candR = crtCandidate->timePosMs + crtCandidate->length;
            if (candR > timelineLengthMs) timelineLengthMs = candR;
            if (candR > tracks[track].length) tracks[track].length = candR;
            crtCandidate = crtCandidate->nextObject;
            continue;
        };
        insertAfter = crtCandidate;
        crtCandidate = crtCandidate->nextObject;
    }
    //if (!crtCandidate) {
    //    insert_timestamp = tracks[track].length;
    //}
    if (insertAfter) {
        obj->nextObject = insertAfter->nextObject;
        insertAfter->nextObject = obj;
        if (insertAfter == tracks[track].last) tracks[track].last = obj;
    } else {
        // Try to append last if it exists
        if (tracks[track].last) {
            tracks[track].last->nextObject = obj;
        }
        tracks[track].last = obj;
        if (!tracks[track].first) {
            tracks[track].first = tracks[track].last;
        }
        debugf(DEBUG_VERBOSE, "Appended to track %u! first: %p last: %p", track, tracks[track].first, tracks[track].last);
    }
    obj->timePosMs = insert_timestamp;
    
    if (obj->timePosMs + obj->length > timelineLengthMs) timelineLengthMs = obj->timePosMs + obj->length;
    if (obj->timePosMs + obj->length > tracks[track].length) tracks[track].length = obj->timePosMs + obj->length;


    return 0;
}

void timeline_heartbeat() {
    if (previewPlaying && timelineLengthMs > 0) {
        uint64_t now = current_time_ms();
        if (!lastTimelineEpoch) lastTimelineEpoch = now;
        crtTimelineMs += (now - lastTimelineEpoch);
        crtTimelineMs %= timelineLengthMs;
        lastTimelineEpoch = now;
    } else {
        lastTimelineEpoch = 0;
    }
}

// Same as insertTimelineObj but tries to automatically pick a track.
void insertTimelineObjFree(struct TimelineObject* obj) {
    for (uint8_t track = 0; track<MAX_TRACKS; track++) {
        struct TimelineObject* crtTrack = tracks[track].first;
        while (crtTrack) {
            uint32_t trackL = crtTrack->timePosMs;
            uint32_t trackR = trackL + crtTrack->length;
            if (trackL < crtTimelineMs && trackR > crtTimelineMs) break;
            crtTrack = crtTrack->nextObject;
        }
        if (!crtTrack) {
            insertTimelineObj(obj, track);
            return;
        }
    }
    insertTimelineObj(obj, 0);
}
