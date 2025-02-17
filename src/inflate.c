#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bit_reader.h"
#include "inflate.h"



#define INFLATE_END_OF_BLOCK    256U
#define INFLATE_CODE_COUNT      289U


static unsigned int uncompressedBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size);


extern int inflate(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length) {
    if (!uncompressed) {
        return INFLATE_NO_OUTPUT;
    }

    *uncompressed_length = 0;
    if (!compressed) {
        return INFLATE_SUCCESS;
    }

    size_t uncompressed_size = compressed_length * sizeof(**uncompressed) / 2;
    *uncompressed = malloc(uncompressed_size);
    if (!*uncompressed) {
        return INFLATE_NO_MEMORY;
    }

    BitReader bit_reader = {
        .compressed = compressed,
        .current_byte = compressed,
#ifdef INFLATE_CAREFUL
        .compressed_end = compressed + compressed_length,
#endif /* INFLATE_CAREFUL */
        .bit_buffer = 0,
        .bit_buffer_count = 0
    };

    /* Process blocks. */
    unsigned int final_block = 0;
    unsigned int block_type = 0;
    do {
        final_block = getBits(&bit_reader, 1);
        block_type = getBits(&bit_reader, 2);
#ifdef INFLATE_CAREFUL
        if (final_block == (unsigned int)-1 || block_type == (unsigned int)-1) {
            return INFLATE_COMPRESSED_INCOMPLETE;
        }
#endif /* INFLATE_CAREFUL */

        unsigned int result = INFLATE_SUCCESS;
        switch (block_type) {
            case 0: // Non-compressed block.
                result = uncompressedBlock(&bit_reader, uncompressed, uncompressed_length, &uncompressed_size);
                if (result) {
                    return result;
                }
                
                break;
            case 1: // Fixed Huffman encoding.
                unsigned int fixed_codes[INFLATE_CODE_COUNT];
                memset(fixed_codes, -1, INFLATE_CODE_COUNT);
                size_t i = 0;
                for (; i <= 143; ++i) {
                    fixed_codes[i + 48] = 8;
                }
                for (; i <= 255; ++i) {
                    fixed_codes[i + 400] = 9;
                }
                for (; i <= 279; ++i) {
                    fixed_codes[i] = 7;
                }
                for (; i <= 287; ++i) {
                    fixed_codes[i + 192] = 8;
                }

                break;
            case 2: // Dynamic Huffman encoding.
                break;
            case 3:
                return INFLATE_INVALID_BLOCK_TYPE;
            default:
                break;
        }
    } while (!final_block);

    return INFLATE_SUCCESS;
}

static unsigned int uncompressedBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size) {
    nextByte(&bit_reader);
    fillBuffer(&bit_reader);
    unsigned int block_length = getBits(&bit_reader, 16);
    unsigned int Nblock_length = getBits(&bit_reader, 16);
#ifdef INFLATE_CAREFUL
    if (block_length == (unsigned int)-1 || Nblock_length == (unsigned int)-1) {
        return INFLATE_COMPRESSED_INCOMPLETE;
    } else if (block_length != ~Nblock_length) {
        return INFLATE_WRONG_BLOCK_LENGTH;
    }
#endif /* INFLATE_CAREFUL */
    
    if (block_length + *uncompressed_length > *uncompressed_size) {
        *uncompressed_size *= 2;
        *uncompressed = realloc(*uncompressed, *uncompressed_size * sizeof(**uncompressed));
        if (!*uncompressed) {
            return INFLATE_NO_MEMORY;
        }
    }

#ifdef INFLATE_CAREFUL
    /* If not enough bytes remain, copy as many as possible and return error code. */
    if (bit_reader->current_byte + block_length >= bit_reader->compressed_end) {
        memcpy(*uncompressed + *uncompressed_length, bit_reader->current_byte, bit_reader->compressed_end - bit_reader->current_byte);
        *uncompressed_length += bit_reader->compressed_end - bit_reader->current_byte;
        *uncompressed = realloc(*uncompressed, *uncompressed_length * sizeof(**uncompressed)); // There is no reason this reallocation would fail.

        return INFLATE_COMPRESSED_INCOMPLETE;
    }
#endif /* INFLATE_CAREFUL */

    memcpy(*uncompressed + *uncompressed_length, bit_reader->current_byte, block_length);
    bit_reader->current_byte += block_length;
    *uncompressed_length += block_length;
#ifdef INFLATE_CAREFUL
    bit_reader->current_byte += block_length;
#endif /* INFLATE_CAREFUL */
}
