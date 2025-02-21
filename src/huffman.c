#include <stdlib.h>
#include <string.h>

#include "huffman.h"
#include "bit_reader.h"
#include "inflate.h"
#include "inflate_internal.h"



unsigned int* huffman_table(const size_t* code_lengths, const size_t code_count, size_t* max_code_length) {
    /*========================================================================================*\
    
    Algorithm explanation will follow as the current algorithm is likely to be improved/changed.

    \*========================================================================================*/


    /* Determine how often each code length is encountered. */
    size_t code_lenght_frequency[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    for (size_t i = 0; i < code_count; ++i) {
        ++code_lenght_frequency[code_lengths[i]];
    }
    code_lenght_frequency[0] = 0;

    /* Determine lowest possible code for each code length. (RFC 1951) */
    unsigned int code = 0;
    unsigned int next_code[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    size_t max_code_length_local = 0;
    for (size_t i = 1; i <= INFLATE_MAX_CODE_LENGTH; ++i) {
        code = (code + code_lenght_frequency[i - 1]) << 1;
        next_code[i] = code;
        if (code_lenght_frequency[i]) {
            max_code_length_local = i;
        }
    }

    /* Initialise Huffman table. */
    size_t table_size = 1U << max_code_length_local;
    unsigned int* table = malloc(table_size * sizeof(*table));
    memset(table, INFLATE_GENERAL_FAILURE, table_size * sizeof(*table));

    /* Map codes to values. */
    size_t current_length = 0;
    for (size_t i = 0; i < code_count; ++i) {
        current_length = code_lengths[i];
        if (current_length) {
            table[next_code[current_length] << max_code_length_local - current_length] = (unsigned int)current_length << 16 | (unsigned int)i;
            ++next_code[current_length];
        }
    }

    /* Map binary strings that or not codes to values. */
    unsigned int current_code = table[0] & 0xFFFF;
    current_length = table[0] >> 16;
    for (size_t i = 0; i < table_size; ++i) {
        if (table[i] == INFLATE_GENERAL_FAILURE) {
            table[i] = current_length << 16 | current_code;
        } else {
            current_code = table[i] & 0xFFFF;
            current_length = table[i] >> 16;
        }
    }

    if (max_code_length) {
        *max_code_length = max_code_length_local;
    }

    return table;
}

unsigned int get_value(BitReader* bit_reader, const unsigned int* table, const size_t max_code_length) {
    unsigned int bit = peek_bits(bit_reader, 1);
    if (bit == INFLATE_GENERAL_FAILURE)
        return INFLATE_GENERAL_FAILURE;

    unsigned int code = 0;
    for (size_t i = 0; i < max_code_length;) {
        code = code << 1 | bit;
    
        ++i;
        bit = peek_bits(bit_reader, i + 1);
        if (bit == INFLATE_GENERAL_FAILURE) {
            code <<= max_code_length - i;

            if (table[code] >> 16 > i) {
                return INFLATE_GENERAL_FAILURE;
            }

            break;
        } else {
            bit >>= i;
        }
    }
    get_bits(bit_reader, table[code] >> 16);

    return table[code] & 0xFFFF;
}
