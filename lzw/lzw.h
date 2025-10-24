#ifndef LZWSTREAM_H
#define LZWSTREAM_H

#include <stdint.h>

#define MAX_CODE_LEN 12
#define FIRST_CODE 256
#define MAX_CODES (1 << MAX_CODE_LEN)

typedef struct {
    unsigned char suffixChar;
    unsigned int prefixCode;
} decode_dictionary_t;

typedef struct {
    unsigned int nextCode;
    unsigned char currentCodeLen;
    unsigned int lastCode;
    unsigned char c;
    decode_dictionary_t dictionary[MAX_CODES - FIRST_CODE];
    unsigned char stack[MAX_CODES];
    int stackSize;
} lzw_decoder_t;


void LZWInit(lzw_decoder_t *dec, unsigned int firstCode, int minCodeLen);
const unsigned char* LZWFeedCode(lzw_decoder_t *dec, unsigned int code, int *outSize);

#endif
