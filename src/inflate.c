#include <stdlib.h>
#include <string.h>

#include "inflate.h"
#include "inflate_internal.h"
#include "bit_reader.h"
#include "huffman.h"



/* Deflate defined constants (RFC 1951). */
#define INFLATE_END_OF_BLOCK_LITERAL    256U
#define INFLATE_LITERAL_CODE_COUNT      288UL
#define INFLATE_HIGHEST_LITERAL_CODE    285UL
#define INFLATE_DISTANCE_CODE_COUNT     32UL
#define INFLATE_HIGHEST_DISTANCE_CODE   29UL
#define INFLATE_CODE_LENGTH_CODE_COUNT  19UL


typedef struct Decompressed {
    unsigned char*  bytes;
    size_t          length;
    size_t          size;
} Decompressed;


/**
 * @brief Process a deflate uncompressed block. In case of an error, processes as much compressed data as possible and returns an error code.
 * 
 * @param bit_reader Contains compressed data.
 * @param decompressed Contains decompressed data.
 * @return int
 */
static int uncompressed_block(BitReader* bit_reader, Decompressed* decompressed);

/**
 * @brief Decompress a deflate block compressed with fixed Huffman encoding. Returns an error code in case of an error.
 * 
 * @param bit_reader Contains compressed data.
 * @param decompressed Contains decompressed data.
 * @return int
 */
static int fixed_huffman_block(BitReader* bit_reader, Decompressed* decompressed);

/**
 * @brief Decompress a deflate block compressed with dynamic Huffman encoding. Returns an error code in case of an error.
 * 
 * @param bit_reader Contains compressed data.
 * @param decompressed Contains decompressed data.
 * @return int
 */
static int dynamic_huffman_block(BitReader* bit_reader, Decompressed* decompressed);

/**
 * @brief Decompresses the data of a Huffman encoded deflate block. Returns an error code in case of an error.
 * 
 * @param bit_reader Contains compressed data.
 * @param decompressed Contains decompressed data.
 * @param literal_table Literal Huffman table.
 * @param distance_table Distance Huffman table.
 * @param max_literal_code_length Length of a longest literal code used to create @a literal_table.
 * @param max_distance_code_length Length of a longest distance code used to create @a distance_table.
 * @return int
 */
static int process_encoded_block_data(BitReader* bit_reader, Decompressed* decompressed, const unsigned int* literal_table, const unsigned int* distance_table, const size_t max_literal_code_length, const size_t max_distance_code_length);

/**
 * @brief Handle an lz77 value as described by the deflate standard (RFC 1951). Returns an error code in case of an error.
 * 
 * @param bit_reader Contains compressed data.
 * @param decompressed Contains decompressed data.
 * @param distance_table Distance Huffman table.
 * @param max_distance_code_length Length of a longest distance code used to create @a distance_table.
 * @param value Lz77 value.
 * @return int
 */
static int lz77(BitReader* bit_reader, Decompressed* decompressed, const unsigned int* distance_table, const size_t max_distance_code_length, unsigned int value);

/**
 * @brief Allocate enough memory to @a decompressed so it can contain at least @a new_length bytes. Returns error code in case of an error.
 * 
 * @param decompressed Contains decompressed data.
 * @param new_length Length @a decompressed should be able to contain.
 * @return int
 */
static int expand_decompressed(Decompressed* decompressed, const size_t new_length);


extern int inflate(const unsigned char* compressed, const size_t compressed_length, unsigned char** decompressed, size_t* decompressed_length) {
    int result = INFLATE_SUCCESS;

    if (!decompressed)
        return INFLATE_NO_OUTPUT;

    Decompressed decompressed_local = { 0 };

    if (compressed && compressed_length) {
        /* Initialise decompression output. */
        decompressed_local.size = compressed_length;
        decompressed_local.bytes = malloc(decompressed_local.size);
        if (!*decompressed_local.bytes)
            return INFLATE_NO_MEMORY;

        BitReader bit_reader = {
            .compressed = compressed,
            .current_byte = compressed,
            .compressed_end = compressed + compressed_length,
            .bit_buffer = 0,
            .bit_buffer_count = 0
        };

        /* Process blocks. */
        unsigned int final_block = 0;
        unsigned int block_type = 0;
        do {
            final_block = get_bits(&bit_reader, 1);
            block_type = get_bits(&bit_reader, 2);
            if (final_block == INFLATE_GENERAL_FAILURE || block_type == INFLATE_GENERAL_FAILURE)
                return INFLATE_COMPRESSED_INCOMPLETE;

            switch (block_type) {
                case 0: // Non-compressed block.
                    result = uncompressed_block(&bit_reader, &decompressed_local);
                    break;
                case 1: // Fixed Huffman encoding.
                    result = fixed_huffman_block(&bit_reader, &decompressed_local);
                    break;
                case 2: // Dynamic Huffman encoding.
                    result = dynamic_huffman_block(&bit_reader, &decompressed_local);
                    break;
                case 3: // Unused block type.
                    result = INFLATE_INVALID_BLOCK_TYPE;
                    break;
                default:
                    break;
            }

            if (result)
                break;
        } while (!final_block);
    }

    if (decompressed_local.length)
        decompressed_local.bytes = realloc(decompressed_local.bytes, decompressed_local.length);
    *decompressed = decompressed_local.bytes;
    if (decompressed_length)
        *decompressed_length = decompressed_local.length;

    return result;
}

static int uncompressed_block(BitReader* bit_reader, Decompressed* decompressed) {
    int result = INFLATE_SUCCESS;

    /* Go to nearest byte boundary and fill bit buffer. For the entirety of this function, bit_reader will be byte aligned. */
    next_byte(bit_reader);
    fill_buffer(bit_reader);

    /* Check whether block length and its one's complement match (LEN and NLEN in RFC 1951). */
    unsigned int block_length = get_bits(bit_reader, 16);
    unsigned int Nblock_length = get_bits(bit_reader, 16);
    if (block_length == INFLATE_GENERAL_FAILURE || Nblock_length == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    else if ((block_length & 0xFFFFU) != (~Nblock_length & 0xFFFFU))
        return INFLATE_BLOCK_LENGTH_UNCERTAIN;

    /* Empty bit buffer. */
    bit_reader->bit_buffer = 0;
    bit_reader->current_byte -= bit_reader->bit_buffer_count >> 3; // Go back number of bytes still in buffer (at most 4 at this point).
    bit_reader->bit_buffer_count = 0;

    /* If not enough bytes remain in bit reader, copy as many as possible. */
    if (bit_reader->current_byte + block_length >= bit_reader->compressed_end) {
        block_length = bit_reader->compressed_end - bit_reader->current_byte;
        result = INFLATE_COMPRESSED_INCOMPLETE;
    }

    /* Expand current output array if necessary. */
    result = expand_decompressed(decompressed, decompressed->length + block_length);

    memcpy(decompressed->bytes + decompressed->length, bit_reader->current_byte, block_length);
    bit_reader->current_byte += block_length;
    decompressed->length += block_length;

    return result;
}

static int fixed_huffman_block(BitReader* bit_reader, Decompressed* decompressed) {
    int result = INFLATE_SUCCESS;

    /* Initialise literal code lengths as defined by the deflate standard (RFC 1951). */
    size_t i = 0;
    size_t literal_code_lengths[INFLATE_LITERAL_CODE_COUNT];
    for (; i <= 143; ++i)
        literal_code_lengths[i] = 8;
    for (; i <= 255; ++i)
        literal_code_lengths[i] = 9;
    for (; i <= 279; ++i)
        literal_code_lengths[i] = 7;
    for (; i <= 287; ++i)
        literal_code_lengths[i] = 8;

    /* Calculate literal Huffman table. */
    unsigned int* literal_table = huffman_table(literal_code_lengths, INFLATE_LITERAL_CODE_COUNT, NULL);

    /* Initialise distance code lengths as defined by the deflate standard (RFC 1951). */
    size_t distance_code_lengths[INFLATE_DISTANCE_CODE_COUNT];
    for (i = 0; i < INFLATE_DISTANCE_CODE_COUNT; ++i)
        distance_code_lengths[i] = 5;

    /* Calculate distance Huffman table. */
    unsigned int* distance_table = huffman_table(distance_code_lengths, INFLATE_DISTANCE_CODE_COUNT, NULL);

    /* Process block data. */
    result = process_encoded_block_data(bit_reader, decompressed, literal_table, distance_table, 9, 5);
    
    free(literal_table);
    free(distance_table);

    return result;
}

static unsigned int code_length_code_length_order[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int dynamic_huffman_block(BitReader* bit_reader, Decompressed* decompressed) {
    /* Read information defining this deflate block. (The obtained values correspond to HLIT, HDIST and HCLEN in RFC 1951.) */
    fill_buffer(bit_reader);
    size_t literal_code_count = get_bits(bit_reader, 5);
    size_t distance_code_count = get_bits(bit_reader, 5);
    size_t code_length_code_count = get_bits(bit_reader, 4);
    if (literal_code_count == INFLATE_GENERAL_FAILURE || distance_code_count == INFLATE_GENERAL_FAILURE || code_length_code_count == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    literal_code_count += 257;
    distance_code_count += 1;
    code_length_code_count += 4;

    /* Initialise code length code lengths as defined by the deflate standard (RFC 1951). */
    size_t code_length_code_lengths[INFLATE_CODE_LENGTH_CODE_COUNT] = { 0 };
    unsigned int code_length_value = 0;
    for (size_t i = 0; i < code_length_code_count; ++i) {
        code_length_value = code_length_code_length_order[i];
        code_length_code_lengths[code_length_value] = get_bits(bit_reader, 3);

        if (code_length_code_lengths[code_length_value] == INFLATE_GENERAL_FAILURE)
            return INFLATE_COMPRESSED_INCOMPLETE;
    }
    
    /* Calculate code length Huffman table. */
    size_t max_code_length_code_length = 0;
    unsigned int* code_length_table = huffman_table(code_length_code_lengths, INFLATE_CODE_LENGTH_CODE_COUNT, &max_code_length_code_length);

    /* code_lengths contains both the literal code lengths and the distance code lengths. */
    size_t code_lengths_count = literal_code_count + distance_code_count;
    size_t* code_lengths = malloc(code_lengths_count * sizeof(*code_lengths));
    if (!code_lengths)
        return INFLATE_NO_MEMORY;
    size_t* literal_code_lengths = code_lengths;
    size_t* distance_code_lengths = code_lengths + literal_code_count;

    /* Decode literal and distance code lengths. */
    size_t i = 0;
    unsigned int value = 0;
    while (i < code_lengths_count) {
        fill_buffer(bit_reader);
        value = get_value(bit_reader, code_length_table, max_code_length_code_length);
        if (value == INFLATE_GENERAL_FAILURE)
            return INFLATE_COMPRESSED_INCOMPLETE;

        if (value <= 15) {
            code_lengths[i] = value;
            ++i;
        } else {
            unsigned int repeat_length = 0;
            if (value == 16) {
                repeat_length = get_bits(bit_reader, 2);
                if (repeat_length == INFLATE_GENERAL_FAILURE)
                    return INFLATE_COMPRESSED_INCOMPLETE;

                repeat_length += 3;
                for (size_t j = 0; j < repeat_length; ++j) {
                    code_lengths[i] = code_lengths[i - 1];
                    ++i;
                }
            } else if (value == 17) {
                repeat_length = get_bits(bit_reader, 3);
                if (repeat_length == INFLATE_GENERAL_FAILURE)
                    return INFLATE_COMPRESSED_INCOMPLETE;

                repeat_length += 3;
                for (size_t j = 0; j < repeat_length; ++j) {
                    code_lengths[i] = 0;
                    ++i;
                }
            } else { // value == 18.
                repeat_length = get_bits(bit_reader, 7);
                if (repeat_length == INFLATE_GENERAL_FAILURE)
                    return INFLATE_COMPRESSED_INCOMPLETE;
                
                repeat_length += 11;
                for (size_t j = 0; j < repeat_length; ++j) {
                    code_lengths[i] = 0;
                    ++i;
                }
            }
        }
    }
    free(code_length_table);
    
    /* Calculate literal and distance Huffman tables. */
    size_t max_literal_code_length = 0;
    size_t max_distance_code_length = 0;
    unsigned int* literal_table = huffman_table(literal_code_lengths, literal_code_count, &max_literal_code_length);
    unsigned int* distance_table = huffman_table(distance_code_lengths, distance_code_count, &max_distance_code_length);
    free(code_lengths); // This implicitly frees literal_code_lengths and distance_code_lengths.

    /* Process block data. */
    int result = INFLATE_SUCCESS;
    result = process_encoded_block_data(bit_reader, decompressed, literal_table, distance_table, max_literal_code_length, max_distance_code_length);

    free(literal_table);
    free(distance_table);

    return result;
}

static int process_encoded_block_data(BitReader* bit_reader, Decompressed* decompressed, const unsigned int* literal_table, const unsigned int* distance_table, const size_t max_literal_code_length, const size_t max_distance_code_length) {
    int result = INFLATE_SUCCESS;

    /* Decode all Huffman codes in deflate block as specified in the deflate standard (RFC 1951). */
    unsigned int value = 0;
    do {
        fill_buffer(bit_reader);
        value = get_value(bit_reader, literal_table, max_literal_code_length);
        if (value == INFLATE_GENERAL_FAILURE) {
            result = INFLATE_COMPRESSED_INCOMPLETE;
            break;
        }

        if (value < INFLATE_END_OF_BLOCK_LITERAL) {
            expand_decompressed(decompressed, decompressed->length + 1);
            decompressed->bytes[decompressed->length] = value;
            ++decompressed->length;
        } else if (value > INFLATE_HIGHEST_LITERAL_CODE) {
            result = INFLATE_VALUE_NOT_ALLOWED;
            break;
        } else if (value > INFLATE_END_OF_BLOCK_LITERAL) {
            result = lz77(bit_reader, decompressed, distance_table, max_distance_code_length, value);
            if (result)
                break;
        }
    } while (value != INFLATE_END_OF_BLOCK_LITERAL);

    return result;
}


static unsigned int length_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

static unsigned int length_extra_bits[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0
};

static unsigned int distance_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0
};

static unsigned int distance_extra_bits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0
};

static int lz77(BitReader* bit_reader, Decompressed* decompressed, const unsigned int* distance_table, const size_t max_distance_code_length, unsigned int value) {
    value -= 257; // Translate value to length code.

    fill_buffer(bit_reader);

    /* Calculate length of back reference. */
    unsigned int length = get_bits(bit_reader, length_extra_bits[value]);
    if (length == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    length += length_base[value];

    /* Calculate distance to back reference. */
    value = get_value(bit_reader, distance_table, max_distance_code_length);
    if (value == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    else if (value > INFLATE_HIGHEST_DISTANCE_CODE)
        return INFLATE_VALUE_NOT_ALLOWED;

    unsigned int distance = get_bits(bit_reader, distance_extra_bits[value]);
    if (distance == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    distance += distance_base[value];

    if (distance > decompressed->length)
        return INFLATE_INVALID_LZ77;
    
    int result = INFLATE_SUCCESS;
    result = expand_decompressed(decompressed, decompressed->length + length);
    if (result)
        return result;

    /* Copy length bytes, byte by byte, so the destination may overlap with the byte array that will be copied. */
    for (unsigned char* byte = decompressed->bytes + decompressed->length; byte < decompressed->bytes + decompressed->length + length; ++byte)
        *byte = *(byte - distance);
    decompressed->length += length;

    return result;
}

static int expand_decompressed(Decompressed* decompressed, const size_t new_length) {
    while (new_length > decompressed->size) {
        decompressed->size *= 2;
        decompressed->bytes = realloc(decompressed->bytes, decompressed->size);

        if (!decompressed->bytes)
            return INFLATE_NO_MEMORY;
    }

    return INFLATE_SUCCESS;
}
