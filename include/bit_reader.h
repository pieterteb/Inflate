#ifndef BIT_READER_H
#define BIT_READER_H

#include <stddef.h>
#include <stdint.h>

#include "inflate.h"



#if defined(UINT64_MAX)
#   define INFLATE_64_BIT
    typedef unsigned long long BufferType;
#elif defined(UINT32_MAX)
#   define INFLATE_32_BIT
    typedef unsigned long BufferType;
#endif /* defined(UINT64_MAX) */


typedef struct BitReader {
    const unsigned char*  compressed;
    const unsigned char*  current_byte;
#ifdef INFLATE_CAREFUL
    const unsigned char*  compressed_end; // Points to first character after compressed.
#endif /* INFLATE_CAREFUL */

    BufferType      bit_buffer;
    size_t          bit_buffer_count;
} BitReader;


/**
 * @brief Refills bit buffer in steps of 32 or 16 bits depending on the platform.
 * 
 * @param bit_reader Contains compressed data.
 */
void fillBuffer(BitReader* bit_reader);

/**
 * @brief Get @a count bits from @a bit_reader and consume them if available. Else returns -1.
 * 
 * @param bit_reader Contains compressed data.
 * @param count Number of bits. At most 16 bits can be requested.
 * @return unsigned int 
 */
unsigned int getBits(BitReader* bit_reader, size_t count);

/**
 * @brief Read @a count bits from @a bit_reader if available. Else returns -1.
 * 
 * @param bit_reader Contains compressed data.
 * @param count Number of bits. At most 16 bits can be requested.
 * @return unsigned int 
 */
unsigned int peekBits(BitReader* bit_reader, size_t count);

/**
 * @brief Move @a bit_reader to next byte boundary.
 * 
 * @param bit_reader Contains compressed data.
 */
void nextByte(BitReader* bit_reader);



#endif /* BIT_READER_H */
