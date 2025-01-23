/*
https://datatracker.ietf.org/doc/html/rfc1951#section-7
*/

#ifndef INFLATE_H
#define INFLATE_H


/* 
If not defined, inflate() is optimised with the assumption that input is error-free deflate. 
If defined, invalid deflate will be handled by returning an error or warning code, corresponding to the fault encountered first.
*/
#define INFLATE_CAREFUL


/* Inflate success and error codes. */
enum {
    INFLATE_SUCCESS,
    INFLATE_NO_OUTPUT,
    INFLATE_NO_MEMORY,
    INFLATE_COMPRESSED_INCOMPLETE,
    INFLATE_INVALID_BLOCK_TYPE,
    INFLATE_WRONG_BLOCK_LENGTH,
};


#ifdef INFLATE_CAREFUL
#   define NEED_BITS(num_bits) \
        do { \
            while (bit_buffer_count < (num_bits)) { \
                if (!bytes_left) { \
                    return INFLATE_COMPRESSED_INCOMPLETE; \
                } \
                --bytes_left; \
                bit_buffer |= (uint32_t)*current_byte << bit_buffer_count; \
                ++current_byte; \
                bit_buffer_count += 8; \
            } \
        } while (0)
#else
#   define NEED_BITS(num_bits) \
        do { \
            while (bit_buffer_count < (num_bits)) { \
                bit_buffer |= (uint32_t)*current_byte << bit_buffer_count; \
                ++current_byte; \
                bit_buffer_count += 8; \
            } \
        } while (0)
#endif /* INFLATE_CAREFUL */

#define GET_BITS(num_bits) (bit_buffer & ((1U << num_bits) - 1))

#define DROP_BITS(num_bits) \
    do { \
        bit_buffer >>= num_bits; \
        bit_buffer_count -= num_bits; \
    } while (0)

#define NEXT_BYTE() \
    do { \
        bit_buffer >>= (8 - (bit_buffer_count & 7)) & 7; \
        bit_buffer_count -= (8 - (bit_buffer_count & 7)) & 7; \
    } while (0)


#include <stddef.h>



extern int inflate(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length);



#endif /* INFLATE_H */
