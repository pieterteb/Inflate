#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"
#include "bit_reader.h"
#include "inflate.h"



unsigned int* huffmanTable(size_t* code_lengths, size_t code_count) {
    size_t code_lenght_frequency[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    for (size_t i = 0; i < code_count; ++i) {
        ++code_lenght_frequency[code_lengths[i]];
    }

    /* Determine lowest possible code for each code length. */
    unsigned int code = 0;
    unsigned int next_code[INFLATE_MAX_CODE_LENGTH + 1] = { 0 };
    size_t max_code_length = 0;
    for (size_t i = 1; i <= INFLATE_MAX_CODE_LENGTH; ++i) {
        code = (code + code_lenght_frequency[i - 1]) << 1;
        next_code[i] = code;
        if (code_lenght_frequency[i]) {
            max_code_length = i;
        }
    }

    size_t table_size = 1U << max_code_length;
    unsigned int* table = malloc(table_size * sizeof(*table));
    memset(table, INFLATE_UNUSED_CODE, table_size * sizeof(*table));

    /* Map codes to values. */
    size_t current_length = 0;
    for (size_t i = 0; i < code_count; ++i) {
        current_length = code_lengths[i];
        if (current_length) {
            table[next_code[current_length] << max_code_length - current_length] = (unsigned int)current_length << 16 | (unsigned int)i;
            ++next_code[current_length];
        }
    }

    unsigned int current_code = table[0] & 0xFFFF;
    current_length = table[0] >> 16;
    for (size_t i = 0; i < table_size; ++i) {
        if (table[i] == INFLATE_UNUSED_CODE) {
            table[i] = current_length << 16 | current_code;
        } else {
            current_code = table[i] & 0xFFFF;
            current_length = table[i] >> 16;
        }
    }

    return table;
}

unsigned int getValue(BitReader* bit_reader, unsigned int* table, size_t max_code_length) {
    unsigned int bit = peekBits(bit_reader, 1);
#ifdef INFLATE_CAREFUL
    if (bit == (unsigned int)-1) {
        return (unsigned int)-1;
    }
#endif /* INFLATE_CAREFUL */

    unsigned int code = 0;
    for (size_t i = 0; i < max_code_length;) {
        code = code << 1 | bit;
    
        ++i;
        bit = peekBits(bit_reader, i + 1);
        if (bit == (unsigned int)-1) {
            code <<= max_code_length - i;

#ifdef INFLATE_CAREFUL
            if (table[code] >> 16 > i) {
                return (unsigned int)-1;
            }
#endif /* INFLATE_CAREFUL */

            break;
        } else {
            bit >>= i;
        }
    }
    getBits(bit_reader, table[code] >> 16);

    return table[code] & 0xFFFF;
}
