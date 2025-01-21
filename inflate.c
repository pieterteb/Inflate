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


static char consumeBit(InflateStream* const inflate_stream);


extern int inflate(const char* restrict compressed, const size_t compressed_length, char** const restrict uncompressed, size_t* const restrict uncompressed_length) {
    if (!compressed) {
        *uncompressed_length = 0;
        return 0;
    }

    InflateStream inflate_stream = (InflateStream){ compressed, compressed_length, *uncompressed, 0, 0b00000001, 0 };


}

static char consumeBit(InflateStream* const inflate_stream) {
    if (inflate_stream->byte_position == inflate_stream->compressed_length) {
        return INFLATE_END_OF_COMPRESSED;
    }

    unsigned char bit = inflate_stream->compressed[inflate_stream->byte_position] & inflate_stream->bit_position;

    if (inflate_stream->bit_position == 0b10000000) {
        ++inflate_stream->byte_position;
        inflate_stream->bit_position = 0b00000001;
    } else {
        inflate_stream->bit_position <<= 1;
    }

    return bit;
}
