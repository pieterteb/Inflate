#include <stdbool.h>
#include <string.h>

#include "inflate.h"
#include "bit_reader.h"
#include "huffman.h"
#include "inflate_internal.h"



/* Decompress a deflate block compressed with dynamic Huffman encoding. */
static int dynamic_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/* Decompresses the data of a Huffman encoded deflate block. */
static int process_encoded_block_data(BitReader* bit_reader, const unsigned int* literal_table, const unsigned int* distance_table, const size_t max_literal_code_length, const size_t max_distance_code_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);

/* Handle an lz77 value as described by the deflate standard (RFC 1951). */
static int lz77(BitReader* bit_reader, const unsigned int* distance_table, const size_t max_distance_code_length, unsigned int value, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);


extern int tinflate(const uint8_t* compressed, const size_t compressed_length, uint8_t* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    int result = INFLATE_SUCCESS;

    if (!decompressed)
        return INFLATE_NO_OUTPUT;

    if (compressed && compressed_length) {
        /* Initializing buffer. */
        const uint8_t* compressed_next = compressed;
        const uint8_t* compressed_end = compressed + compressed_length;
        Buffer buffer = 0;
        uint32_t buffer_count = 0;

        uint8_t* decompressed_next = decompressed;
        uint8_t* decompressed_end = decompressed + decompressed_max_length;

        /* Initializing inflator struct. */
        struct Inflator inflator = { 0 };

        unsigned literal_code_count = 0;
        unsigned distance_code_count = 0;

        /* Process blocks. */
        unsigned final_block = 0;   // BFINAL: 1 bit.
        unsigned block_type = 0;    // BTYPE: 2 bits.
        do {
            FILL_BUFFER();
            if (buffer_count < 1 + 2)
                return INFLATE_COMPRESSED_INCOMPLETE;
            final_block = buffer & BITMASK(1);
            block_type = buffer >> 1 & BITMASK(2);
            switch (block_type) {
                case INFLATE_BLOCKTYPE_UNCOMPRESSED:
                    /* Align bit stream to next byte boundary. */
                    buffer_count -= 3; // For BFINAL and BTYPE.
                    compressed_next -= buffer_count >> 3;
                    buffer = 0;
                    buffer_count = 0;

                    if (compressed_end - compressed_next < 4)
                        return INFLATE_COMPRESSED_INCOMPLETE;

                    /* Check block length integrity. */
                    uint16_t block_length = *(uint16_t*)compressed_next;
                    uint16_t Nblock_length = *(uint16_t*)(compressed_next + 2);
                    compressed_next += 4;
                    if (block_length != (uint16_t)~Nblock_length)
                        return INFLATE_BLOCK_LENGTH_UNCERTAIN;

                    if (block_length > compressed_end - compressed_next)
                        return INFLATE_COMPRESSED_INCOMPLETE;
                    if (block_length > decompressed_end - decompressed_next)
                        return INFLATE_DECOMPRESSED_OVERFLOW;

                    memcpy(decompressed_next, compressed_next, block_length);
                    compressed_next += block_length;
                    decompressed_next += block_length;

                    break;
                case INFLATE_BLOCKTYPE_STATIC_HUFFMAN:
                    CONSUME_BITS(3); // For BFINAL and BTYPE.

                    if (!inflator.static_table_loaded) {
                        inflator.static_table_loaded = true;
                        
                        /* Initialise literal code lengths as defined by the deflate standard (RFC 1951). */
                        unsigned i = 0;
                        for (; i < 144; ++i)
                            inflator.u.s.code_lengths[i] = 8;
                        for (; i < 256; ++i)
                            inflator.u.s.code_lengths[i] = 9;
                        for (; i < 280; ++i)
                            inflator.u.s.code_lengths[i] = 7;
                        for (; i < INFLATE_LITERAL_CODE_COUNT; ++i)
                            inflator.u.s.code_lengths[i] = 8;
                        
                        /* Initialise distance code lengths as defined by the deflate standard (RFC 1951). */
                        for (; i < INFLATE_LITERAL_CODE_COUNT + INFLATE_DISTANCE_CODE_COUNT; ++i)
                            inflator.u.s.code_lengths[i] = 5;
                        
                        literal_code_count = INFLATE_LITERAL_CODE_COUNT;
                        distance_code_count = INFLATE_DISTANCE_CODE_COUNT;
                    }

                    break;
                case INFLATE_BLOCKTYPE_DYNAMIC_HUFFMAN:
                    static const uint8_t code_length_code_length_order[INFLATE_CODE_LENGTH_CODE_COUNT] = {
                        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
                    };

                    if (buffer_count < 1 + 2 + 5 + 5 + 4 + 3)
                        return INFLATE_COMPRESSED_INCOMPLETE;
                    literal_code_count = 257 + (buffer >> 3 & BITMASK(5));
                    distance_code_count = 1 + (buffer >> 8 & BITMASK(5));
                    unsigned code_length_code_count = 4 + (buffer >> 13 & BITMASK(4));

                    inflator.u.code_length_code_lengths[code_length_code_length_order[0]] = buffer >> 17 & BITMASK(3);
                    CONSUME_BITS(20);
                    FILL_BUFFER();
                    if (buffer_count < 3 * (code_length_code_count - 1))
                        return INFLATE_COMPRESSED_INCOMPLETE;
                    
                    /* Get code length code lengths and construct code length table. */
                    unsigned i = 1;
                    for (; i < code_length_code_count; ++i) {
                        inflator.u.code_length_code_lengths[code_length_code_length_order[i]] = buffer & BITMASK(3);
                        CONSUME_BITS(3);
                    }
                    for (; i < INFLATE_CODE_LENGTH_CODE_COUNT; ++i)
                        inflator.u.code_length_code_lengths[code_length_code_length_order[i]] = 0;

                    result = build_code_length_table(&inflator);
                    if (result)
                        return result;

                    i = 0;
                    do {
                        if (buffer_count < INFLATE_MAX_CODE_LENGTH_CODE_LENGTH + 7)
                            FILL_BUFFER();
                        
                        uint32_t entry = inflator.u.s.code_length_table[buffer & BITMASK(INFLATE_MAX_CODE_LENGTH_CODE_LENGTH)];
                        buffer >>= (uint8_t)entry;
                        buffer_count -= (uint8_t)entry;
                        unsigned code = entry >> 16;

                        unsigned repeat_value = 0;
                        unsigned repeat_count = 0;
                        if (code < 16) {
                            inflator.u.s.code_lengths[i] = code;
                            ++i;
                        } else if (code == 16) {
                            if (buffer_count < 2)
                                return INFLATE_COMPRESSED_INCOMPLETE;
                            if (!i)
                                return INFLATE_INVALID_HUFFMAN_CODE;
                            
                            
                            repeat_value = inflator.u.s.code_lengths[i - 1];
                            repeat_count = 3 + (buffer & BITMASK(2));
                            CONSUME_BITS(2);
                            inflator.u.s.code_lengths[i] = repeat_value;
                            inflator.u.s.code_lengths[i + 1] = repeat_value;
                            inflator.u.s.code_lengths[i + 2] = repeat_value;
                            inflator.u.s.code_lengths[i + 3] = repeat_value;
                            inflator.u.s.code_lengths[i + 4] = repeat_value;
                            inflator.u.s.code_lengths[i + 5] = repeat_value;
                            i += repeat_count;
                        } else if (code == 17) {
                            if (buffer_count < 3)
                                return INFLATE_COMPRESSED_INCOMPLETE;

                            repeat_count = 3 + (buffer & BITMASK(3));
                            CONSUME_BITS(3);
                            inflator.u.s.code_lengths[i] = 0;
                            inflator.u.s.code_lengths[i + 1] = 0;
                            inflator.u.s.code_lengths[i + 2] = 0;
                            inflator.u.s.code_lengths[i + 3] = 0;
                            inflator.u.s.code_lengths[i + 4] = 0;
                            inflator.u.s.code_lengths[i + 5] = 0;
                            inflator.u.s.code_lengths[i + 6] = 0;
                            inflator.u.s.code_lengths[i + 7] = 0;
                            inflator.u.s.code_lengths[i + 8] = 0;
                            i += repeat_count;
                        } else {
                            if (buffer_count < 7)
                                return INFLATE_COMPRESSED_INCOMPLETE;

                            repeat_count = 11 + (buffer & BITMASK(7));
                            CONSUME_BITS(7);
                            memset(&inflator.u.s.code_lengths[i], 0, repeat_count * sizeof(inflator.u.s.code_lengths[i]));
                            i += repeat_count;
                        }
                    } while (i < literal_code_count + distance_code_count);

                    break;
                default:
                    return INFLATE_INVALID_BLOCK_TYPE;
            }

            if (block_type == 1 || block_type == 2) {
                result = build_literal_table(&inflator, literal_code_count);
                if (result)
                    return result;
                result = build_distance_table(&inflator, literal_code_count, distance_code_count);
                if (result)
                    return result;

                
            }
        } while (!final_block);
    }

    return result;
}



static int dynamic_huffman_block(BitReader* bit_reader, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    /* Read information defining this deflate block. (The obtained values correspond to HLIT, HDIST and HCLEN in RFC 1951.) */
    fill_buffer(bit_reader);
    if (bit_reader->buffer_count < 5 + 5 + 4 + 3)
        return INFLATE_COMPRESSED_INCOMPLETE;
    size_t literal_code_count = get_bits(bit_reader, 5);
    size_t distance_code_count = get_bits(bit_reader, 5);
    size_t code_length_code_count = get_bits(bit_reader, 4);
    literal_code_count += 257;
    distance_code_count += 1;
    code_length_code_count += 4;

    /* Initialise code length code lengths as defined by the deflate standard (RFC 1951). */
    size_t code_length_code_lengths[INFLATE_CODE_LENGTH_CODE_COUNT] = { 0 };
    unsigned int code_length_value = code_length_code_length_order[0];
    code_length_code_lengths[code_length_value] = get_bits(bit_reader, 3);
    fill_buffer(bit_reader);
    if (bit_reader->buffer_count < 3 * (INFLATE_CODE_LENGTH_CODE_COUNT - 1))
        return INFLATE_COMPRESSED_INCOMPLETE;
    for (size_t i = 1; i < code_length_code_count; ++i) {
        code_length_value = code_length_code_length_order[i];
        code_length_code_lengths[code_length_value] = get_bits(bit_reader, 3);
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

        if (value <= 15) {
            code_lengths[i] = value;
            ++i;
        } else {
            unsigned int repeat_length = 0;
            if (value == 16) {
                if (bit_reader->buffer_count < 2) {
                    free(code_length_table);
                    free(code_lengths);
                    return INFLATE_COMPRESSED_INCOMPLETE;
                }
                repeat_length = get_bits(bit_reader, 2) + 3;
                for (size_t j = 0; j < repeat_length; ++j) {
                    code_lengths[i] = code_lengths[i - 1];
                    ++i;
                }
            } else if (value == 17) {
                if (bit_reader->buffer_count < 3) {
                    free(code_length_table);
                    free(code_lengths);
                    return INFLATE_COMPRESSED_INCOMPLETE;
                }
                repeat_length = get_bits(bit_reader, 3);
                if (repeat_length == INFLATE_GENERAL_FAILURE)
                    return INFLATE_COMPRESSED_INCOMPLETE;

                repeat_length += 3;
                for (size_t j = 0; j < repeat_length; ++j) {
                    code_lengths[i] = 0;
                    ++i;
                }
            } else { // value == 18.
                if (bit_reader->buffer_count < 7) {
                    free(code_length_table);
                    free(code_lengths);
                    return INFLATE_COMPRESSED_INCOMPLETE;
                }
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
