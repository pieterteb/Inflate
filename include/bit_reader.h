#ifndef BIT_READER_H
#define BIT_READER_H

#include <stddef.h>

#include "inflate.h"



/* If C standard is younger than ANSI C, we use the <stdint.h> header. Else we check for a 16 bit wide integer type. */
#if defined(__STDC_VERSION__)
#   define INFLATE_64_BIT

#   include <stdint.h>

    typedef unsigned long long Buffer;
    typedef uint32_t Reader;
#else
#   define INFLATE_32_BIT

#   include <limits.h>

    typedef unsigned long Buffer;
#   if UCHAR_MAX == 0xFFFF
#   define INFLATE_16_BIT
        typedef unsigned char Reader;
#   elif SHRT_MAX == 0xFFFF
#   define INFLATE_16_BIT
        typedef unsigned short Reader;
#   elif UINT_MAX == 0xFFFF
#   define INFLATE_16_BIT
        typedef unsigned int Reader;
#   endif
#endif /* defined(UINT64_MAX) */


typedef struct BitReader {
    const unsigned char*    compressed;     // Pointer to compressed data.
    const unsigned char*    current_byte;
    const unsigned char*    compressed_end; // Pointer to first character after compressed.

    Buffer                  bit_buffer;
    size_t                  bit_buffer_count;
} BitReader;


/**
 * @brief Refills bit buffer in steps of 32 or 16 bits depending on the platform.
 * 
 * @param bit_reader Contains compressed data.
 */
void fill_buffer(BitReader* bit_reader);

/**
 * @brief Read @a count bits from @a bit_reader and consume them if available. Else returns INFLATE_GENERAL_FAILURE.
 * 
 * @param bit_reader Contains compressed data.
 * @param count Number of bits. At most 16 bits can be requested in a single function call.
 * @return unsigned int
 */
unsigned int get_bits(BitReader* bit_reader, size_t count);

/**
 * @brief Read @a count bits from @a bit_reader without consuming the bits if available. Else returns INFLATE_GENERAL_FAILURE.
 * 
 * @param bit_reader Contains compressed data.
 * @param count Number of bits. At most 16 bits can be requested in a single function call.
 * @return unsigned int
 */
unsigned int peek_bits(BitReader* bit_reader, size_t count);

/**
 * @brief Move @a bit_reader to nearest byte boundary. If @a bit_reader is already on a byte boundary, does nothing.
 * 
 * @param bit_reader Contains compressed data.
 */
void next_byte(BitReader* bit_reader);



#endif /* BIT_READER_H */
