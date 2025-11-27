#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image32.h"
#include "debug.h"
#include "timeline/timeline.h"
#include "graphics/graphics.h"

#define OBJECT_HEADER 'O'
#define TRACK_SWITCH_HEADER 'T'
#define STRING_HEADER 'S'
#define PROJECTSAVEFAIL {fclose(fd); free(scratch); return 0;}
#define PROJECTLOADFAIL {fclose(fd); free(scratch); if(strBuffer){for(int i=0;i<strBufSize;i++)free(strBuffer[i]);free(strBuffer);} return 0;}

extern struct Timeline tracks[MAX_TRACKS];
extern uint32_t crtTimelineMs;
extern uint32_t timelineLengthMs;
extern uint32_t fileWidthPx;
extern uint32_t fileHeightPx;

extern struct LoadedFile* getMetadata(char* filename);

uint8_t saveProject(char* filePath) {
    uint32_t* scratch = malloc(64);
    if (!scratch) return 0;

    FILE* fd = fopen(filePath, "wb");
    if (!fd) {free(scratch); return 0;}
    if(!fwrite("Grr0", 4, 1, fd)) PROJECTSAVEFAIL
        if(!fwrite(&crtTimelineMs, 4, 1, fd)) PROJECTSAVEFAIL

            uint32_t stringId = 0;

    for (uint8_t track = 0; track < MAX_TRACKS - 1; track++) {
        struct TimelineObject* crtObj = tracks[track].first;
        while (crtObj) {
            uint8_t hdr = STRING_HEADER;
            if (!fwrite(&hdr, 1, 1, fd)) PROJECTSAVEFAIL
                if (!fwrite("\x01", 1, 1, fd)) PROJECTSAVEFAIL
                    uint32_t fileLen = strlen(crtObj->fileName) + 1;
            if (!fwrite(&fileLen, 4, 1, fd)) PROJECTSAVEFAIL
                if (!fwrite(crtObj->fileName, fileLen, 1, fd)) PROJECTSAVEFAIL

                    memset(scratch, 0, 64);
            hdr = OBJECT_HEADER;
            if (!fwrite(&hdr, 1, 1, fd)) PROJECTSAVEFAIL
                scratch[0] = crtObj->x;
            scratch[1] = crtObj->y;
            scratch[2] = crtObj->width;
            scratch[3] = crtObj->height;
            scratch[4] = crtObj->timePosMs;
            scratch[5] = crtObj->length;
            scratch[6] = stringId;
            if (!fwrite(scratch, 7*4, 1, fd)) PROJECTSAVEFAIL

                stringId++;
            crtObj = crtObj->nextObject;
        }
        uint8_t hdr = TRACK_SWITCH_HEADER;
        if (!fwrite(&hdr, 1, 1, fd)) PROJECTSAVEFAIL
    }
    fclose(fd);
    free(scratch);
    return 1;
}

uint8_t loadProject(char* filePath) {
    uint32_t* scratch = malloc(64);
    if (!scratch) return 0;

    FILE* fd = fopen(filePath, "rb");
    if (!fd) {free(scratch); return 0;}
    if (!fread(scratch, 4, 1, fd)) PROJECTSAVEFAIL
        if (scratch[0] != (('G'<<0)|('r'<<8)|('r'<<16)|('0'<<24))) PROJECTSAVEFAIL
            if (!fread(&crtTimelineMs, 4, 1, fd)) PROJECTSAVEFAIL

                char** strBuffer = NULL;
    uint32_t strBufSize = 0;
    uint8_t header = 0;
    uint8_t currentTrack = 0;

    while (fread(&header, 1, 1, fd)) {
        switch (header) {
            case STRING_HEADER: {
                uint8_t strCount;
                if (!fread(&strCount, 1, 1, fd)) PROJECTLOADFAIL

                    char** newBuf = realloc(strBuffer, (strBufSize + strCount) * sizeof(char*));
                if (!newBuf) PROJECTLOADFAIL
                    strBuffer = newBuf;

                for (uint8_t strCrt = 0; strCrt < strCount; strCrt++) {
                    uint32_t strLength = 0;
                    if (!fread(&strLength, 4, 1, fd)) PROJECTLOADFAIL
                        char* str = malloc(strLength);
                    if (!str) PROJECTLOADFAIL
                        if (!fread(str, strLength, 1, fd)) PROJECTLOADFAIL
                            strBuffer[strBufSize++] = str;
                }
                break;
            }
            case OBJECT_HEADER: {
                if (!fread(scratch, 7*4, 1, fd)) PROJECTLOADFAIL

                    struct TimelineObject* newObj = malloc(sizeof(struct TimelineObject));
                if (!newObj) PROJECTLOADFAIL

                    newObj->x = scratch[0];
                newObj->y = scratch[1];
                newObj->width = scratch[2];
                newObj->height = scratch[3];
                newObj->timePosMs = scratch[4];
                newObj->length = scratch[5];
                uint32_t strId = scratch[6];

                if (strId >= strBufSize) PROJECTLOADFAIL
                    newObj->fileName = strdup(strBuffer[strId]);
                newObj->metadata = getMetadata(newObj->fileName);
                newObj->effectsList = NULL;
                newObj->nextObject = NULL;
                newObj->track = currentTrack;

                if (!tracks[currentTrack].first) {
                    tracks[currentTrack].first = newObj;
                } else {
                    struct TimelineObject* last = tracks[currentTrack].first;
                    while (last->nextObject) last = last->nextObject;
                    last->nextObject = newObj;
                }
                break;
            }
            case TRACK_SWITCH_HEADER:
                currentTrack++;
                if (currentTrack >= MAX_TRACKS) PROJECTLOADFAIL
                    break;
            default:
                PROJECTLOADFAIL
        }
    }

    if (strBuffer) {
        for (uint32_t i = 0; i < strBufSize; i++) free(strBuffer[i]);
        free(strBuffer);
    }
    fclose(fd);
    free(scratch);
    return 1;
}

void saveGIF() {

}
