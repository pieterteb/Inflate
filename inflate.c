#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "inflate.h"



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

    uint32_t bit_buffer = 0;
    size_t bit_buffer_count = 0;
#ifdef INFLATE_CAREFUL
    size_t bytes_left = compressed_length;
#endif /* INFLATE_CAREFUL */
    unsigned char* current_byte = compressed;

    /* Process blocks. */
    uint32_t block_header = 0;
    do {
        NEED_BITS(3);
        block_header = GET_BITS(3);
        DROP_BITS(3);
        switch (block_header & 3) {
            case 0: // Non-compressed block.
                NEXT_BYTE();
#ifdef INFLATE_CAREFUL
                NEED_BITS(32);
                if (bit_buffer & 0x00ff != ~(bit_buffer >> 16)) {
                    return INFLATE_WRONG_BLOCK_LENGTH;
                }
                uint16_t block_length = (uint16_t)GET_BITS(16);
                DROP_BITS(32);
#else
                NEED_BITS(16);
                uint16_t block_length = (uint16_t)bit_buffer;
                DROP_BITS(16);
                /* Skip NLEN. */
                current_byte += 2;
#endif /* INFLATE_CAREFUL */
                
                if (block_length + *uncompressed_length > uncompressed_size) {
                    uncompressed_size *= 2;
                    *uncompressed = realloc(*uncompressed, uncompressed_size * sizeof(**uncompressed));
                    if (!*uncompressed) {
                        return INFLATE_NO_MEMORY;
                    }
                }

#ifdef INFLATE_CAREFUL
                if (bytes_left < block_length) {
                    return INFLATE_COMPRESSED_INCOMPLETE;
                }
#endif /* INFLATE_CAREFUL */

                memcpy(&(*uncompressed)[*uncompressed_length], current_byte, block_length);
                current_byte += block_length;
                *uncompressed_length += block_length;
#ifdef INFLATE_CAREFUL
                bytes_left -= block_length;
#endif /* INFLATE_CAREFUL */
                
                continue;
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
