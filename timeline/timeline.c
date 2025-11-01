#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../image32.h"
#include "timeline.h"

uint32_t timelineObjects;
uint32_t timelineMaxMs;
struct TimelineObject** sequentialAccess = 0;
struct TimelineObject* timeline = 0;

uint32_t crtTimelineMs;
uint32_t fileWidthPx = 1200;
uint32_t fileHeightPx = 600;

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
    return 0;
}