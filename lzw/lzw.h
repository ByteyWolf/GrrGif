#include <stdint.h>
#include <stddef.h>

#define LZW_MAX_CODE 4096
#define LZW_MAX_STACK 4096
#define LZW_OUTPUT_BUFFER_SIZE 65536

typedef struct {
    uint8_t suffix[LZW_MAX_CODE];
    uint16_t prefix[LZW_MAX_CODE];
    
    uint8_t min_code_size;
    uint16_t clear_code;
    uint16_t eoi_code;
    uint16_t next_code;
    uint16_t max_code;
    uint8_t code_size;

    uint32_t bit_buffer;
    uint8_t bits_in_buffer;

    int prev_code;

    uint8_t stack[LZW_MAX_STACK];

    uint8_t output_buffer[LZW_OUTPUT_BUFFER_SIZE];
    size_t output_pos;

    int initialized;
    int finished;
} lzw_state_t;

void lzw_init(lzw_state_t *state, uint8_t min_code_size);

uint8_t* lzw_feed(lzw_state_t *state, const uint8_t *chunk, size_t chunk_size, 
                  size_t *decoded_size);