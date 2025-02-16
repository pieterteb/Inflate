#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"
#include "inflate.h"



uint16_t* huffmanTable(size_t* code_lengths, size_t code_count, size_t* table_size, size_t* max_code_length) {
    size_t code_lenght_frequency[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    for (size_t i = 0; i < code_count; ++i) {
        ++code_lenght_frequency[code_lengths[i]];
    }

    /* Determine lowest possible code for each code length. */
    uint16_t code = 0;
    uint16_t next_code[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    for (size_t i = 1; i <= INFLATE_MAX_CODE_LENGTH; ++i) {
        code = (code + code_lenght_frequency[i - 1]) << 1;
        next_code[i] = code;
        if (code_lenght_frequency[i]) {
            *max_code_length = i;
        }
    }

    *table_size = next_code[*max_code_length] + code_lenght_frequency[*max_code_length];
    uint16_t* table = malloc(*table_size * sizeof(*table));
    memset(table, INFLATE_UNUSED_CODE, *table_size * sizeof(*table));

    /* Map codes to values. */
    size_t current_length = 0;
    for (size_t i = 0; i < code_count; ++i) {
        current_length = code_lengths[i];
        if (current_length) {
            table[next_code[current_length] << *max_code_length - current_length] = (uint16_t)i;
            ++next_code[current_length];
        }
    }

    uint16_t current_code = table[*table_size - 1];
    for (size_t i = 0; i < *table_size; ++i) {
        if (table[i] == INFLATE_UNUSED_CODE) {
            table[i] = current_code;
        } else {
            current_code = table[i];
        }
    }

    return table;
}
