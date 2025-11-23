// TODO: Rewrite everthing

#include <string.h>
#include <stdint.h>
#include "lzw.h"


static void init_table(lzw_state_t *state) {
    uint16_t i;
    uint16_t num_colors = 1 << state->min_code_size;
    
    for (i = 0; i < num_colors; i++) {
        state->suffix[i] = (uint8_t)i;
        state->prefix[i] = 0xFFFF;
    }
    
    state->clear_code = num_colors;
    state->eoi_code = state->clear_code + 1;
    state->next_code = state->eoi_code + 1;
    state->code_size = state->min_code_size + 1;
    state->max_code = (1 << state->code_size);
    state->prev_code = -1;
}

void lzw_init(lzw_state_t *state, uint8_t min_code_size) {
    memset(state, 0, sizeof(lzw_state_t));
    
    state->min_code_size = min_code_size;
    state->initialized = 1;
    state->finished = 0;
    init_table(state);
}

static uint16_t get_next_code(lzw_state_t *state, const uint8_t **data, 
                              size_t *remaining) {
    while (state->bits_in_buffer < state->code_size && *remaining > 0) {
        state->bit_buffer |= ((uint32_t)(**data)) << state->bits_in_buffer;
        state->bits_in_buffer += 8;
        (*data)++;
        (*remaining)--;
    }
    
    if (state->bits_in_buffer < state->code_size) {
        return 0xFFFF;
    }
    
    uint32_t mask = (1 << state->code_size) - 1;
    uint16_t code = state->bit_buffer & mask;
    state->bit_buffer >>= state->code_size;
    state->bits_in_buffer -= state->code_size;
    
    return code;
}

static int decode_string(lzw_state_t *state, uint16_t code, uint8_t *first_char) {
    int i = 0;
    uint16_t c = code;
    
    while (c >= state->clear_code && i < LZW_MAX_STACK) {
        if (c >= LZW_MAX_CODE) {
            return -1;
        }
        state->stack[i++] = state->suffix[c];
        c = state->prefix[c];
        
        if (i >= LZW_MAX_STACK) {
            return -1;
        }
    }
    
    if (c >= (1 << state->min_code_size) && c != state->clear_code) {
        return -1;
    }
    
    state->stack[i++] = state->suffix[c];
    *first_char = state->suffix[c];
    
    return i;
}

static int output_string(lzw_state_t *state, int stack_size) {
    if (state->output_pos + stack_size > LZW_OUTPUT_BUFFER_SIZE) {
        return -1;
    }
    
    for (int i = stack_size - 1; i >= 0; i--) {
        state->output_buffer[state->output_pos++] = state->stack[i];
    }
    
    return 0;
}

uint8_t* lzw_feed(lzw_state_t *state, const uint8_t *chunk, 
                  size_t chunk_size, size_t *decoded_size) {
    if (!state->initialized || state->finished) {
        *decoded_size = 0;
        return NULL;
    }
    
    state->output_pos = 0;
    const uint8_t *data = chunk;
    size_t remaining = chunk_size;
    
    while (remaining > 0 || state->bits_in_buffer >= state->code_size) {
        uint16_t code = get_next_code(state, &data, &remaining);
        
        if (code == 0xFFFF) break;
        
        if (code == state->clear_code) {
            init_table(state);
            continue;
        }
        
        if (code == state->eoi_code) {
            state->finished = 1;
            break;
        }
        
        if (code >= LZW_MAX_CODE) {
            *decoded_size = 0;
            return NULL;
        }
        
        uint8_t first_char = 0;
        int stack_size;
        
        if (state->prev_code == -1) {
            if (code >= state->next_code) {
                *decoded_size = 0;
                return NULL;
            }
            stack_size = decode_string(state, code, &first_char);
            if (stack_size < 0 || output_string(state, stack_size) < 0) {
                *decoded_size = 0;
                return NULL;
            }
            state->prev_code = code;
            continue;
        }
        
        uint16_t in_code = code;
        
        if (code < state->next_code) {
            stack_size = decode_string(state, code, &first_char);
            if (stack_size < 0 || output_string(state, stack_size) < 0) {
                *decoded_size = 0;
                return NULL;
            }
        } else if (code == state->next_code) {
            stack_size = decode_string(state, state->prev_code, &first_char);
            if (stack_size < 0 || output_string(state, stack_size) < 0) {
                *decoded_size = 0;
                return NULL;
            }
            if (state->output_pos >= LZW_OUTPUT_BUFFER_SIZE) {
                *decoded_size = 0;
                return NULL;
            }
            state->output_buffer[state->output_pos++] = first_char;
        } else {
            *decoded_size = 0;
            return NULL;
        }
        
        if (state->next_code < LZW_MAX_CODE) {
            state->prefix[state->next_code] = state->prev_code;
            state->suffix[state->next_code] = first_char;
            state->next_code++;
            
            if (state->next_code >= state->max_code && state->code_size < 12) {
                state->code_size++;
                state->max_code = (1 << state->code_size);
            }
        }
        
        state->prev_code = in_code;
    }
    
    *decoded_size = state->output_pos;
    return state->output_pos > 0 ? state->output_buffer : NULL;
}

