#ifndef BIT_READER_H
#define BIT_READER_H


#include <stddef.h>
#include <stdint.h>

#include "inflate.h"



typedef uint64_t Buffer;
typedef uint32_t Reader;


typedef struct BitReader {
    const unsigned char*    compressed;     // Pointer to compressed data.
    const unsigned char*    current_byte;
    const unsigned char*    compressed_end; // Pointer to first character after compressed.

    Buffer                  bit_buffer;
    size_t                  bit_buffer_count;
} BitReader;


/**
 * @brief Adds 32 bits to buffer if possible.
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
void byte_align(BitReader* bit_reader);



#endif /* BIT_READER_H */
