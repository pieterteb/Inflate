#ifndef BIT_READER_H
#define BIT_READER_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "inflate.h"



#if defined(UINT64_MAX)
#   define INFLATE_64_BIT
    typedef uint64_t BufferType;
#elif defined(UINT32_MAX)
#   define INFLATE_32_BIT
    typedef uint32_t BufferType;
#endif /* defined(UINT64_MAX) */


typedef struct BitReader {
    unsigned char*  compressed;
    unsigned char*  current_byte;
#ifdef INFLATE_CAREFUL
    unsigned char*  compressed_end; // Points to first character after compressed.
#endif /* INFLATE_CAREFUL */

    BufferType      bit_buffer;
    size_t          bit_buffer_count;
} BitReader;


void fillBuffer(BitReader* bit_reader);

unsigned int getBits(BitReader* bit_reader, size_t count);

unsigned int peekBits(BitReader* bit_reader, size_t count);

void nextByte(BitReader* bit_reader);



#endif /* BIT_READER_H */
