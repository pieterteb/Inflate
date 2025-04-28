#include "bit_reader.h"

#include <string.h>

#include "inflate_internal.h"



void fill_buffer(BitReader* bit_reader) {
    if (bit_reader->bit_buffer_count <= 32 && bit_reader->current_byte + 4 < bit_reader->compressed_end) {
        bit_reader->bit_buffer |= (Buffer)*(Reader*)bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 32;
        bit_reader->current_byte += 4;
    }
}

unsigned int get_bits(BitReader* bit_reader, size_t count) {
    /* Add more bits to buffer if necessary. */
    while (bit_reader->bit_buffer_count < count && bit_reader->current_byte < bit_reader->compressed_end) {
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
    }

    /* Get requested bits. */
    unsigned int value = INFLATE_GENERAL_FAILURE;
    if (bit_reader->bit_buffer_count >= count) {
        value = bit_reader->bit_buffer & (unsigned int)((1U << count) - 1);
        bit_reader->bit_buffer >>= count;
        bit_reader->bit_buffer_count -= count;
    }
    
    return value;
}

unsigned int peek_bits(BitReader* bit_reader, size_t count) {
    /* Add more bits to buffer if necessary. */
    while (bit_reader->bit_buffer_count < count && bit_reader->current_byte < bit_reader->compressed_end) {
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
    }

    unsigned int value = INFLATE_GENERAL_FAILURE;
    if (bit_reader->bit_buffer_count >= count)
        value = bit_reader->bit_buffer & (unsigned int)((1U << count) - 1);

    return value;
}

void byte_align(BitReader* bit_reader) {
    bit_reader->bit_buffer = 0;
    bit_reader->current_byte -= bit_reader->bit_buffer_count >> 3; // Go back number of bytes still in buffer (at most 4 at this point).
    bit_reader->bit_buffer_count = 0;
}
