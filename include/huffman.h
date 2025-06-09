/* 
* This huffman decoding method is a slightly modified version of
* https://github.com/ebiggers/libdeflate/blob/master/lib/deflate_decompress.c
*/

#ifndef HUFFMAN_H
#define HUFFMAN_H


#include <stdbool.h>
#include <stdint.h>

#include "inflate_internal.h"



#define CODE_LENGTH_TABLE_BITS      7
#define CODE_LENGTH_ENOUGH          128
#define LITERAL_TABLE_BITS          11
#define LITERAL_ENOUGH              2342
#define DISTANCE_TABLE_BITS         8
#define DISTANCE_ENOUGH             402


struct Inflator {
    union {
        uint8_t code_length_code_lengths[INFLATE_CODE_LENGTH_CODE_COUNT];

        struct {
            uint8_t code_lengths[INFLATE_LITERAL_CODE_COUNT + INFLATE_DISTANCE_CODE_COUNT + INFLATE_MAX_LENGTHS_OVERRUN];
            uint32_t code_length_table[CODE_LENGTH_ENOUGH];
        } s;

        uint32_t literal_table[LITERAL_ENOUGH];
    } u;

    uint32_t distance_table[DISTANCE_ENOUGH];
    uint16_t sorted_codes[INFLATE_MAX_CODE_COUNT];

    bool static_table_loaded;
    unsigned literal_table_bits;
};


int build_code_length_table(struct Inflator* inflator);

int build_literal_table(struct Inflator* inflator, unsigned literal_code_count);

int build_distance_table(struct Inflator* inflator, unsigned literal_code_count, unsigned distance_code_count);



#endif /* HUFFMAN_H */
