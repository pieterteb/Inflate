#include <stdlib.h>
#include <string.h>

#include "inflate.h"
#include "inflate_internal.h"
#include "bit_reader.h"
#include "huffman.h"



/* Process a deflate uncompressed block. */
static int uncompressed_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/*  Decompress a deflate block compressed with fixed Huffman encoding. */
static int fixed_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/* Decompress a deflate block compressed with dynamic Huffman encoding. */
static int dynamic_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/* Decompresses the data of a Huffman encoded deflate block. */
static int process_encoded_block_data(BitReader* bit_reader, const unsigned int* literal_table, const unsigned int* distance_table, const size_t max_literal_code_length, const size_t max_distance_code_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/* Handle an lz77 value as described by the deflate standard (RFC 1951). */
static int lz77(BitReader* bit_reader, const unsigned int* distance_table, const size_t max_distance_code_length, unsigned int value, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);


extern int tinflate(const unsigned char* compressed, size_t compressed_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    int result = INFLATE_SUCCESS;

    if (!decompressed)
        return INFLATE_NO_OUTPUT;

    if (compressed && compressed_length) {
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
        fill_buffer(&bit_reader);
        do {
            final_block = get_bits(&bit_reader, 1);
            block_type = get_bits(&bit_reader, 2);
            if (final_block == INFLATE_GENERAL_FAILURE || block_type == INFLATE_GENERAL_FAILURE)
                return INFLATE_COMPRESSED_INCOMPLETE;

            switch (block_type) {
                case 0: // Non-compressed block.
                    result = uncompressed_block(&bit_reader, decompressed, decompressed_length, decompressed_max_length);
                    break;
                case 1: // Fixed Huffman encoding.
                    result = fixed_huffman_block(&bit_reader, decompressed, decompressed_length, decompressed_max_length);
                    break;
                case 2: // Dynamic Huffman encoding.
                    result = dynamic_huffman_block(&bit_reader, decompressed, decompressed_length, decompressed_max_length);
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

    return result;
}

static int uncompressed_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    /* Go to nearest byte boundary and fill bit buffer. For the entirety of this function, bit_reader will be byte aligned. */
    byte_align(bit_reader);
    fill_buffer(bit_reader);

    /* Check whether block length and its one's complement match (LEN and NLEN in RFC 1951). */
    uint16_t block_length = (uint16_t)get_bits(bit_reader, 16);
    uint16_t Nblock_length = (uint16_t)get_bits(bit_reader, 16);
    if (block_length == INFLATE_GENERAL_FAILURE || Nblock_length == INFLATE_GENERAL_FAILURE)
        return INFLATE_COMPRESSED_INCOMPLETE;
    else if (block_length != (~Nblock_length & 0xFFFFU))
        return INFLATE_BLOCK_LENGTH_UNCERTAIN;

    if (bit_reader->current_byte + block_length >= bit_reader->compressed_end)
        return INFLATE_COMPRESSED_INCOMPLETE;
    else if (*decompressed_length + block_length > decompressed_max_length)
        return INFLATE_DECOMPRESSED_OVERFLOW;

    memcpy(decompressed + *decompressed_length, bit_reader->current_byte, block_length);
    bit_reader->current_byte += block_length;
    *decompressed_length += block_length;

    return INFLATE_SUCCESS;
}

static int fixed_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    /* Initialise literal code lengths as defined by the deflate standard (RFC 1951). */
    size_t i = 0;
    size_t literal_code_lengths[INFLATE_LITERAL_CODE_COUNT];
    for (; i < 144; ++i)
        literal_code_lengths[i] = 8;
    for (; i < 256; ++i)
        literal_code_lengths[i] = 9;
    for (; i < 280; ++i)
        literal_code_lengths[i] = 7;
    for (; i < INFLATE_LITERAL_CODE_COUNT; ++i)
        literal_code_lengths[i] = 8;

    /* Initialise distance code lengths as defined by the deflate standard (RFC 1951). */
    size_t distance_code_lengths[INFLATE_DISTANCE_CODE_COUNT];
    for (i = 0; i < INFLATE_DISTANCE_CODE_COUNT; ++i)
        distance_code_lengths[i] = 5;

    /* Calculate Huffman tables. */
    unsigned int* literal_table = huffman_table(literal_code_lengths, INFLATE_LITERAL_CODE_COUNT, NULL);
    unsigned int* distance_table = huffman_table(distance_code_lengths, INFLATE_DISTANCE_CODE_COUNT, NULL);

    /* Process block data. */
    int result = process_encoded_block_data(bit_reader, literal_table, distance_table, 9, 5, decompressed, decompressed_length, decompressed_max_length);
    
    free(literal_table);
    free(distance_table);

    return result;
}

static unsigned int code_length_code_length_order[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int dynamic_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
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
    result = process_encoded_block_data(bit_reader, literal_table, distance_table, max_literal_code_length, max_distance_code_length, decompressed, decompressed_length, decompressed_max_length);

    free(literal_table);
    free(distance_table);

    return result;
}

static int process_encoded_block_data(BitReader* bit_reader, const unsigned int* literal_table, const unsigned int* distance_table, const size_t max_literal_code_length, const size_t max_distance_code_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    /* Decode all Huffman codes in deflate block as specified in the deflate standard (RFC 1951). */
    unsigned int value = 0;
    while(1) {
        fill_buffer(bit_reader);
        value = get_value(bit_reader, literal_table, max_literal_code_length);
        if (value == INFLATE_GENERAL_FAILURE)
            return INFLATE_COMPRESSED_INCOMPLETE;

        if (value == INFLATE_END_OF_BLOCK_LITERAL)
            break;

        if (*decompressed_length == decompressed_max_length)
            return INFLATE_DECOMPRESSED_OVERFLOW;
            
        if (value < INFLATE_END_OF_BLOCK_LITERAL) {
            decompressed[*decompressed_length] = value;
            ++*decompressed_length;
        } else if (value <= INFLATE_HIGHEST_LITERAL_CODE) {
            int result = lz77(bit_reader, distance_table, max_distance_code_length, value, decompressed, decompressed_length, decompressed_max_length);
            if (result)
                break;
        } else {
            return INFLATE_VALUE_NOT_ALLOWED;
        }
    }

    return INFLATE_SUCCESS;
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

static int lz77(BitReader* bit_reader, const unsigned int* distance_table, const size_t max_distance_code_length, unsigned int value, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
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

    if (distance > *decompressed_length)
        return INFLATE_INVALID_LZ77;
    
    /* Copy length bytes, byte by byte, so the destination may overlap with the byte array that will be copied. */
    if (*decompressed_length + length > decompressed_max_length)
        return INFLATE_DECOMPRESSED_OVERFLOW;
    unsigned char* end = decompressed + *decompressed_length + length;
    unsigned char* byte = decompressed + *decompressed_length;
    unsigned char* reference = byte - distance;
    for (; byte < end; ++byte) {
        *byte = *reference;
        ++reference;
    }
    *decompressed_length += length;

    return INFLATE_SUCCESS;
}
