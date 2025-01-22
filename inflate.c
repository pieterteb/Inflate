#include <stddef.h>

#include "inflate.h"



typedef struct InflateStream {
    char*           compressed;
    size_t          compressed_length;

    char*           uncompressed;
    size_t          uncompressed_length;

    unsigned char   bit_position;
    size_t          byte_position;
} InflateStream;


static int consumeBit(InflateStream* const inflate_stream);

static int consumeNumeric(InflateStream* const inflate_stream, const size_t bit_length);


extern int inflate(const unsigned char* restrict compressed, const size_t compressed_length, unsigned char** const restrict uncompressed, size_t* const restrict uncompressed_length) {
    if (!compressed) {
        *uncompressed_length = 0;
        return 0;
    }

    InflateStream inflate_stream = (InflateStream){ compressed, compressed_length, *uncompressed, 0, 0b00000001, 0 };

    /* Process blocks. */
    int final_block = 0;
    int block_type = 0;
    do {
        final_block = consumeBit(&inflate_stream);
        block_type = consumeNumeric(&inflate_stream, 2);
        switch (block_type) {
            case 0:
            case 1:
            case 2:
            case 3:
                return INFLATE_INVALID_BLOCK_TYPE;
            default:
                break;
        }
    } while (!final_block);
}

static int consumeBit(InflateStream* const inflate_stream) {
    if (inflate_stream->byte_position == inflate_stream->compressed_length) {
        return INFLATE_END_OF_COMPRESSED;
    }

    int bit = inflate_stream->compressed[inflate_stream->byte_position] & inflate_stream->bit_position;

    if (inflate_stream->bit_position == 0b10000000) {
        ++inflate_stream->byte_position;
        inflate_stream->bit_position = 0b00000001;
    } else {
        inflate_stream->bit_position <<= 1;
    }

    return bit;
}

static int consumeNumeric(InflateStream* const inflate_stream, const size_t bit_length) {
    int numeric = 0;

    int bit = 0;
    for (size_t i = 0; i <  bit_length; ++i) {
        bit = consumeBit(inflate_stream);
        if (bit == INFLATE_END_OF_COMPRESSED) {
            return INFLATE_END_OF_COMPRESSED;
        }
        numeric |= bit << i;
    }

    return numeric;
}
