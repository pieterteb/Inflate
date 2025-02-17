#include <limits.h>

#include "bit_reader.h"
#include "inflate.h"



void fillBuffer(BitReader* bit_reader) {
#ifdef INFLATE_64_BIT
    if (bit_reader->bit_buffer_count <= 32 && bit_reader->current_byte + 4 < bit_reader->compressed_end) {
#   if defined(UINT32_MAX) && (defined(_M_X64) || defined(__x86_64__))
        bit_reader->bit_buffer |= *(uint32_t*)bit_reader->current_byte << bit_reader->bit_buffer_count;
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
#   endif /* defined(UINT32_MAX) && (defined(_M_X64) || defined(__x86_64__)) */
    }
#elif defined(INFLATE_32_BIT)
    if (bit_reader->bit_buffer_count <= 16 && bit_reader->current_byte + 2 < bit_reader->compressed_end) {
#   if defined(UINT16_MAX) && (defined(_M_IX86) || defined(__x86__))
        bit_reader->bit_buffer |= *(uint16_t*)bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 16;
        bit_reader->current_byte += 2;
#   else
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
#   endif /* defined(UINT16_MAX) && (defined(_M_IX86) || defined(__x86__)) */
    }
#endif /* INFLATE_64_BIT */
}

unsigned int getBits(BitReader* bit_reader, size_t count) {
#ifdef INFLATE_CAREFUL
    while (bit_reader->bit_buffer_count < count && bit_reader->current_byte < bit_reader->compressed_end) {
#else
    while (bit_reader->bit_buffer_count < count) {
#endif /* INFLATE_CAREFUL */
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
    }

#ifdef INFLATE_CAREFUL
    if (bit_reader->bit_buffer_count < count) {
        return -1;
    }
#endif /* INFLATE_CAREFUL */
    unsigned int value = bit_reader->bit_buffer & (unsigned int)((1UL << count) - 1);
    bit_reader->bit_buffer >>= count;
    bit_reader->bit_buffer_count -= count;
    return value;
}

unsigned int peekBits(BitReader* bit_reader, size_t count) {
    #ifdef INFLATE_CAREFUL
    while (bit_reader->bit_buffer_count < count && bit_reader->current_byte < bit_reader->compressed_end) {
#else
    while (bit_reader->bit_buffer_count < count) {
#endif /* INFLATE_CAREFUL */
        bit_reader->bit_buffer |= *bit_reader->current_byte << bit_reader->bit_buffer_count;
        bit_reader->bit_buffer_count += 8;
        ++bit_reader->current_byte;
    }

#ifdef INFLATE_CAREFUL
    if (bit_reader->bit_buffer_count < count) {
        return -1;
    }
#endif /* INFLATE_CAREFUL */

    return bit_reader->bit_buffer & (unsigned int)((1UL << count) - 1);
}

void nextByte(BitReader* bit_reader) {
    bit_reader->bit_buffer >>= bit_reader->bit_buffer_count & 7;
    bit_reader->bit_buffer_count -= bit_reader->bit_buffer_count & 7;
}
