#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "inflate.h"
#include "bit_reader.h"
#include "huffman.h"



#define INFLATE_END_OF_BLOCK    256U
#define INFLATE_CODE_COUNT      288U
#define INFLATE_DISTANCE_COUNT  32U


/**
 * @brief Processes an uncompressed block.
 * 
 * @param bit_reader Contains compressed data.
 * @param uncompressed Contains uncompressed data.
 * @param uncompressed_length Length in bytes of uncompressed data.
 * @param uncompressed_size Size in bytes of uncommpressed memory block.
 * @return int 
 */
static int uncompressedBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size);

/**
 * @brief Processes a block using fixed encoding.
 * 
 * @param bit_reader Contains compressed data.
 * @param uncompressed Contains uncompressed data.
 * @param uncompressed_length Length in bytes of uncompressed data.
 * @param uncompressed_size Size in bytes of uncompressed memory block.
 * @return int 
 */
static int fixedEncodingBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size);

static int handleLz77(unsigned int value, BitReader* bit_reader, unsigned int* code_table, unsigned int* distance_table, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size);


extern int inflate(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length) {
    *uncompressed_length = 0;

    if (!uncompressed) {
        return INFLATE_NO_OUTPUT;
    } else if (!compressed) {
        return INFLATE_SUCCESS;
    }

    size_t uncompressed_size = compressed_length * sizeof(**uncompressed) / 4;
    if (!uncompressed_size) {
        uncompressed_size = 1;
    }
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
                break;
            case 1: // Fixed Huffman encoding.
                result = fixedEncodingBlock(&bit_reader, uncompressed, uncompressed_length, &uncompressed_size);
                break;
            case 2: // Dynamic Huffman encoding.
                break;
            case 3:
                return INFLATE_INVALID_BLOCK_TYPE;
            default:
                break;
        }

        if (result) {
            return result;
        }
    } while (!final_block);

    return INFLATE_SUCCESS;
}

static int uncompressedBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size) {
    nextByte(bit_reader);
    fillBuffer(bit_reader);
    unsigned int block_length = getBits(bit_reader, 16);
    unsigned int Nblock_length = getBits(bit_reader, 16);
#ifdef INFLATE_CAREFUL
    if (block_length == (unsigned int)-1 || Nblock_length == (unsigned int)-1) {
        return INFLATE_COMPRESSED_INCOMPLETE;
    } else if (block_length & 0xFFFF != ~Nblock_length & 0xFFFF) {
        return INFLATE_WRONG_BLOCK_LENGTH;
    }
#endif /* INFLATE_CAREFUL */
    
    while (block_length + *uncompressed_length > *uncompressed_size) {
        *uncompressed_size *= 2;
        *uncompressed = realloc(*uncompressed, *uncompressed_size * sizeof(**uncompressed));
        if (!*uncompressed) {
            return INFLATE_NO_MEMORY;
        }
    }

#ifdef INFLATE_CAREFUL
    /* If not enough bytes remain, copy as many as possible. */
    if (bit_reader->current_byte + block_length >= bit_reader->compressed_end) {
        size_t bytes_left = bit_reader->compressed_end - bit_reader->current_byte;
        memcpy(*uncompressed + *uncompressed_length, bit_reader->current_byte, bytes_left);
        bit_reader->current_byte = bit_reader->compressed_end;
        *uncompressed_length += bytes_left;

        *uncompressed_size = *uncompressed_length;
        *uncompressed = realloc(*uncompressed, *uncompressed_size * sizeof(**uncompressed)); // There is no reason this reallocation would fail.

        return INFLATE_COMPRESSED_INCOMPLETE;
    }
#endif /* INFLATE_CAREFUL */

    memcpy(*uncompressed + *uncompressed_length, bit_reader->current_byte, block_length);
    bit_reader->current_byte += block_length;
    *uncompressed_length += block_length;

    return INFLATE_SUCCESS;
}

static int fixedEncodingBlock(BitReader* bit_reader, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size) {
    size_t i = 0;
    size_t code_lengths[INFLATE_CODE_COUNT];
    for (; i <= 143; ++i) {
        code_lengths[i] = 8;
    }
    for (; i <= 255; ++i) {
        code_lengths[i] = 9;
    }
    for (; i <= 279; ++i) {
        code_lengths[i] = 7;
    }
    for (; i <= 287; ++i) {
        code_lengths[i] = 8;
    }
    unsigned int* code_table = huffmanTable(code_lengths, INFLATE_CODE_COUNT);

    size_t distance_lengths[INFLATE_DISTANCE_COUNT];
    for (i = 0; i < INFLATE_DISTANCE_COUNT; ++i) {
        distance_lengths[i] = 5;
    }
    unsigned int* distance_table = huffmanTable(distance_lengths, INFLATE_DISTANCE_COUNT);

    unsigned int value = 0;
    do {
        value = getValue(bit_reader, code_table, 9);
#ifdef INFLATE_CAREFUL
        if (value == (unsigned int)-1) {
            free(code_table);
            free(distance_table);
            return INFLATE_COMPRESSED_INCOMPLETE;
        }
#endif /* INFLATE_CAREFUL */

        if (value < INFLATE_END_OF_BLOCK) {
            if (*uncompressed_length == *uncompressed_size) {
                *uncompressed_size *= 2;
                *uncompressed = realloc(*uncompressed, *uncompressed_size);

                if (!*uncompressed) {
                    free(code_table);
                    free(distance_table);
                    return INFLATE_NO_MEMORY;
                }
            }

            (*uncompressed)[*uncompressed_length] = value;
            ++*uncompressed_length;
#ifdef INFLATE_CAREFUL
        } else if (value > 285) {
            free(code_table);
            free(distance_table);
            return INFLATE_UNUSED_VALUE;
#endif /* INFLATE_CAREFUL */
        } else if (value > INFLATE_END_OF_BLOCK) {
            int result = handleLz77(value, bit_reader, code_table, distance_table, uncompressed, uncompressed_length, uncompressed_size);
            if (result) {
                free(code_table);
                free(distance_table);
                return result;
            }
        }
    } while (value != INFLATE_END_OF_BLOCK);

    free(code_table);
    free(distance_table);

    return INFLATE_SUCCESS;
}


static unsigned int code_default[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

static unsigned int code_extra_bits[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0
};

static unsigned int distance_default[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0
};

static unsigned int distance_extra_bits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0
};

static int handleLz77(unsigned int value, BitReader* bit_reader, unsigned int* code_table, unsigned int* distance_table, unsigned char** uncompressed, size_t* uncompressed_length, size_t* uncompressed_size) {
    value -= 257;

#ifdef INFLATE_CAREFUL
    unsigned int temp = getBits(bit_reader, code_extra_bits[value]);
    if (temp == (unsigned int)-1) {
        return INFLATE_COMPRESSED_INCOMPLETE;
    }
    unsigned int length = code_default[value] + temp;

    value = getValue(bit_reader, distance_table, 5);
    if (value == (unsigned int)-1) {
        return INFLATE_COMPRESSED_INCOMPLETE;
    } else if (value > 29) {
        return INFLATE_UNUSED_VALUE;
    }
    temp = getBits(bit_reader, distance_extra_bits[value]);
    if (temp == (unsigned int)-1) {
        return INFLATE_COMPRESSED_INCOMPLETE;
    }
    unsigned int distance = distance_default[value] + temp;
#else
    unsigned int length = code_default[value] + getBits(bit_reader, code_extra_bits[value]);
    value = getValue(bit_reader, distance_table, 5);
    unsigned int distance = distance_default[value] + getBits(bit_reader, distance_extra_bits[value]);
#endif /* INFLATE_CAREFUL */

    if (*uncompressed_length + length > *uncompressed_size) {
        *uncompressed_size *= 2;
        *uncompressed = realloc(*uncompressed, *uncompressed_size);

        if (!*uncompressed) {
            return INFLATE_NO_MEMORY;
        }
    }

#ifdef INFLATE_CAREFUL
    if (distance > *uncompressed_length) {
        return INFLATE_INVALID_LZ77;
    }
#endif /* INFLATE_CAREFUL */

    for (size_t i = 0; i < length; ++i) {
        (*uncompressed)[*uncompressed_length + i] = (*uncompressed)[*uncompressed_length - distance + i];
    }
    *uncompressed_length += length;

    return INFLATE_SUCCESS;
}
