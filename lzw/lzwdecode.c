#include "lzw.h"

static unsigned char DecodeStack(unsigned int code, lzw_decoder_t *dec) {
    int top = 0;
    unsigned char firstChar;

    unsigned int cur = code;

    while (cur >= FIRST_CODE) {
        dec->stack[top++] = dec->dictionary[cur - FIRST_CODE].suffixChar;
        cur = dec->dictionary[cur - FIRST_CODE].prefixCode;
    }
    dec->stack[top++] = (unsigned char)cur;
    firstChar = (unsigned char)cur;

    for (int i = 0; i < top / 2; i++) {
        unsigned char tmp = dec->stack[i];
        dec->stack[i] = dec->stack[top - 1 - i];
        dec->stack[top - 1 - i] = tmp;
    }

    dec->stackSize = top;
    return firstChar;
}

void LZWInit(lzw_decoder_t *dec, int minCodeLen) {
    dec->nextCode = FIRST_CODE;
    dec->currentCodeLen = minCodeLen;
    dec->stackSize = 0;
    dec->ready = 0;
}

const unsigned char* LZWFeedCode(lzw_decoder_t *dec, unsigned int code, int *outSize) {
    if (!dec->ready) {
        dec->lastCode = code;
        dec->c = code;
        *outSize = 0;
        return 0;
    }
    unsigned char firstChar;

    if (code < dec->nextCode) {
        firstChar = DecodeStack(code, dec);
    } else {
        unsigned char tmp = dec->c;
        firstChar = DecodeStack(dec->lastCode, dec);
        dec->stack[dec->stackSize++] = tmp;
    }

    if (dec->nextCode < MAX_CODES) {
        dec->dictionary[dec->nextCode - FIRST_CODE].prefixCode = dec->lastCode;
        dec->dictionary[dec->nextCode - FIRST_CODE].suffixChar = dec->c;
        dec->nextCode++;

        if (dec->nextCode >= (1U << dec->currentCodeLen) && dec->currentCodeLen < MAX_CODE_LEN) {
            dec->currentCodeLen++;
        }
    }

    dec->lastCode = code;
    dec->c = firstChar;

    *outSize = dec->stackSize;
    return dec->stack;
}
