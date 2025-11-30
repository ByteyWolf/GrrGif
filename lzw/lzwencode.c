#include <string.h>
#include "lzw.h"

static void init_table(lzw_encode_state_t *state) {
    uint16_t num_colors = 1 << state->min_code_size;

    for (uint16_t i = 0; i < num_colors; i++) {
        state->suffix[i] = (uint8_t)i;
        state->prefix[i] = 0xFFFF;
    }

    state->clear_code = num_colors;
    state->eoi_code = state->clear_code + 1;
    state->next_code = state->eoi_code + 1;
    state->code_size = state->min_code_size + 1;
    state->max_code = (1 << state->code_size);
    state->string_code = 0xFFFF;
    state->has_string = 0;
}

static void write_code(lzw_encode_state_t *state, uint16_t code) {
    state->bit_buffer |= ((uint32_t)code) << state->bits_in_buffer;
    state->bits_in_buffer += state->code_size;

    while (state->bits_in_buffer >= 8) {
        if (state->output_pos < LZW_OUTPUT_BUFFER_SIZE) {
            state->output_buffer[state->output_pos++] =
            (uint8_t)(state->bit_buffer & 0xFF);
        }
        state->bit_buffer >>= 8;
        state->bits_in_buffer -= 8;
    }
}


static int find_code(lzw_encode_state_t *state, uint16_t prefix, uint8_t suffix) {
    if (prefix == 0xFFFF) {
        return suffix;
    }

    for (uint16_t i = state->eoi_code + 1; i < state->next_code; i++) {
        if (state->prefix[i] == prefix && state->suffix[i] == suffix) {
            return i;
        }
    }

    return -1;
}

void lzw_encode_init(lzw_encode_state_t *state, uint8_t min_code_size) {
    memset(state, 0, sizeof(lzw_encode_state_t));
    state->min_code_size = min_code_size;
    state->initialized = 1;
    init_table(state);

    write_code(state, state->clear_code);
}

uint8_t* lzw_encode_feed(lzw_encode_state_t *state, const uint8_t *data,
                         size_t data_size, size_t *encoded_size) {
    if (!state->initialized) {
        *encoded_size = 0;
        return NULL;
    }

    state->output_pos = 0;

    for (size_t i = 0; i < data_size; i++) {
        uint8_t k = data[i];

        if (!state->has_string) {
            // Start new string with this character
            state->string_code = k;
            state->has_string = 1;
            continue;
        }

        // Try to extend current string
        int code = find_code(state, state->string_code, k);

        if (code >= 0) {
            // String+char found in dictionary, extend string
            state->string_code = (uint16_t)code;
        } else {
            // String+char not in dictionary
            // Output the code for current string
            write_code(state, state->string_code);

            // Add string+char to dictionary if there's room
            if (state->next_code < LZW_MAX_CODE) {
                state->prefix[state->next_code] = state->string_code;
                state->suffix[state->next_code] = k;
                state->next_code++;

                // Check if we need to increase code size
                if (state->next_code >= state->max_code && state->code_size < 12) {
                    state->code_size++;
                    state->max_code = (1 << state->code_size);
                }

                // Check if dictionary is full - reset if needed
                if (state->next_code >= LZW_MAX_CODE) {
                    write_code(state, state->clear_code);
                    init_table(state);
                }
            }

            // Start new string with current character
            state->string_code = k;
        }

        // Check if output buffer is getting full
        if (state->output_pos >= LZW_OUTPUT_BUFFER_SIZE - 10) {
            *encoded_size = state->output_pos;
            return state->output_buffer;
        }
    }

    *encoded_size = state->output_pos;
    return state->output_pos > 0 ? state->output_buffer : NULL;
}


uint8_t* lzw_encode_finish(lzw_encode_state_t *state, size_t *encoded_size) {
    state->output_pos = 0;

    if (state->has_string) {
        write_code(state, state->string_code);
        state->has_string = 0;
    }

    write_code(state, state->eoi_code);

    if (state->bits_in_buffer > 0) {
        if (state->output_pos < LZW_OUTPUT_BUFFER_SIZE) {
            state->output_buffer[state->output_pos++] =
            (uint8_t)(state->bit_buffer & 0xFF);
        }
        state->bit_buffer = 0;
        state->bits_in_buffer = 0;
    }

    *encoded_size = state->output_pos;
    return state->output_pos > 0 ? state->output_buffer : NULL;
}
