#include "bit_reader.h"
#include "inflate_internal.h"



void fill_buffer(BitReader* bit_reader) {
/* Attempt to add 4 or 2 bytes to bit buffer depending on the platform. If not enough bytes remain, do nothing. */
#ifdef INFLATE_64_BIT
if (bit_reader->bit_buffer_count <= 32 && bit_reader->current_byte + 4 < bit_reader->compressed_end) {
    /* Check if platform allows unaligned access. */
    #   if defined(_M_X64) || defined(__x86_64__)
        bit_reader->bit_buffer |= (unsigned long long)*(Reader*)bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 32;
        bit_reader->current_byte += 4;
#   else
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
#   endif /* defined(_M_X64) || defined(__x86_64__) */
    }
#elif defined(INFLATE_32_BIT)
    if (bit_reader->bit_buffer_count <= 16 && bit_reader->current_byte + 2 < bit_reader->compressed_end) {
    /* Check if platform allows unaligned access. */
#   if defined(INFLATE_16_BIT) && (defined(_M_IX86) || defined(__x86__))
        bit_reader->bit_buffer |= (unsigned long)*(Reader*)bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 16;
        bit_reader->current_byte += 2;
#   else
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
#   endif /* defined(INFLATE_16_BIT) && (defined(_M_IX86) || defined(__x86__)) */
    }
#endif /* INFLATE_64_BIT */
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
        value = bit_reader->bit_buffer & (unsigned int)((1UL << count) - 1);
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
    if (bit_reader->bit_buffer_count >= count) {
        value = bit_reader->bit_buffer & (unsigned int)((1UL << count) - 1);
    }

    return value;
}

void next_byte(BitReader* bit_reader) {
    bit_reader->bit_buffer >>= bit_reader->bit_buffer_count & 7;
    bit_reader->bit_buffer_count -= bit_reader->bit_buffer_count & 7;
}
