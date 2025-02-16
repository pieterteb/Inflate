#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "inflate.h"



#if defined(UINT64_MAX) || (defined(ULLONG_MAX) && ULLONG_MAX == 0xFFFFFFFFFFFFFFFFULL)
#   define INFLATE_64_BIT
#   define BUFFER_TYPE uint64_t
#else
#   define INFLATE_32_BIT
#   define BUFFER_TYPE uint32_t
#endif /* defined(UINT64_MAX) || (defined(ULLONG_MAX) && ULLONG_MAX == 0xFFFFFFFFFFFFFFFFULL) */


#ifdef INFLATE_64_BIT
#   ifdef INFLATE_CAREFUL
#       define NEED_BITS(num_bits) \
            do { \
                while (bit_buffer_count < (num_bits)) { \
                    if (!bytes_left) { \
                        return INFLATE_COMPRESSED_INCOMPLETE; \
                    } \
                    --bytes_left; \
                    bit_buffer |= (uint64_t)*current_byte << bit_buffer_count; \
                    ++current_byte; \
                    bit_buffer_count += 8; \
                } \
            } while (0)
#   else
#       define NEED_BITS(num_bits) \
            do { \
                while (bit_buffer_count < (num_bits)) { \
                    bit_buffer |= (uint64_t)*current_byte << bit_buffer_count; \
                    ++current_byte; \
                    bit_buffer_count += 8; \
                } \
            } while (0)
#   endif /* INFLATE_CAREFUL */
#else
#   ifdef INFLATE_CAREFUL
#       define NEED_BITS(num_bits) \
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
#   else
#       define NEED_BITS(num_bits) \
            do { \
                while (bit_buffer_count < (num_bits)) { \
                    bit_buffer |= (uint32_t)*current_byte << bit_buffer_count; \
                    ++current_byte; \
                    bit_buffer_count += 8; \
                } \
            } while (0)
#   endif /* INFLATE_CAREFUL */
#endif /* INFLATE_64_BIT */


#define GET_BITS(num_bits) (bit_buffer & ((1U << num_bits) - 1))

#define DROP_BITS(num_bits) \
    do { \
        bit_buffer >>= num_bits; \
        bit_buffer_count -= num_bits; \
    } while (0)

#define EMPTY_BUFFER() \
    do { \
        bit_buffer = 0; \
        bit_buffer_count = 0; \
    } while (0)

#define NEXT_BYTE() \
    do { \
        bit_buffer >>= (8 - (bit_buffer_count & 7)) & 7; \
        bit_buffer_count -= (8 - (bit_buffer_count & 7)) & 7; \
    } while (0)


extern int inflate(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length) {
    if (!uncompressed) {
        return INFLATE_NO_OUTPUT;
    }
    if (!compressed) {
        *uncompressed_length = 0;
        return INFLATE_SUCCESS;
    }

    size_t uncompressed_size = compressed_length * sizeof(**uncompressed) / 2;
    *uncompressed = realloc(*uncompressed, uncompressed_size);
    if (!*uncompressed) {
        *uncompressed_length = 0;
        return INFLATE_NO_MEMORY;
    }

    BUFFER_TYPE bit_buffer = 0;
    size_t bit_buffer_count = 0;
#ifdef INFLATE_CAREFUL
    size_t bytes_left = compressed_length;
#endif /* INFLATE_CAREFUL */
    const unsigned char* current_byte = compressed;

    /* Process blocks. */
    uint32_t block_header = 0;
    do {
        NEED_BITS(3);
        block_header = GET_BITS(3);
        DROP_BITS(3);
        switch (block_header & 3) {
            case 0: // Non-compressed block.
                NEXT_BYTE();
                NEED_BITS(32);
#ifdef INFLATE_CAREFUL
                if (bit_buffer & 0x00ff != ~(bit_buffer >> 16)) {
                    return INFLATE_WRONG_BLOCK_LENGTH;
                }
#endif /* INFLATE_CAREFUL */
                uint16_t block_length = (uint16_t)GET_BITS(16);
                EMPTY_BUFFER();
                
                if (block_length + *uncompressed_length > uncompressed_size) {
                    uncompressed_size *= 2;
                    *uncompressed = realloc(*uncompressed, uncompressed_size * sizeof(**uncompressed));
                    if (!*uncompressed) {
                        return INFLATE_NO_MEMORY;
                    }
                }

#ifdef INFLATE_CAREFUL
                /* If not enough bytes remain, copy as many as possible and return error code. */
                if (bytes_left < block_length) {
                    memcpy(*uncompressed + *uncompressed_length, current_byte, bytes_left);
                    *uncompressed_length += bytes_left;
                    *uncompressed = realloc(*uncompressed, *uncompressed_length * sizeof(**uncompressed)); // There is no reason this reallocation would fail.

                    return INFLATE_COMPRESSED_INCOMPLETE;
                }
#endif /* INFLATE_CAREFUL */

                memcpy(*uncompressed + *uncompressed_length, current_byte, block_length);
                current_byte += block_length;
                *uncompressed_length += block_length;
#ifdef INFLATE_CAREFUL
                bytes_left -= block_length;
#endif /* INFLATE_CAREFUL */
                
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                return INFLATE_INVALID_BLOCK_TYPE;
            default:
                break;
        }
    } while (!(block_header & 4)); // Final block bit.

    return INFLATE_SUCCESS;
}
