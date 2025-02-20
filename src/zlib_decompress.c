#include "zlib_decompress.h"
#include "bit_reader.h"
#include "inflate.h"



extern int zlib_decompress(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length) {
    *uncompressed_length = 0;

    if (!uncompressed) {
        return ZLIB_DECOMPRESS_NO_OUTPUT;
    } else if (!compressed) {
        return ZLIB_DECOMPRESS_SUCCESS;
    }

    BitReader bit_reader = {
        .compressed = compressed,
        .current_byte = compressed,
        .compressed_end = compressed + compressed_length,
        .bit_buffer = 0,
        .bit_buffer_count = 0
    };

    getBits(&bit_reader, 16);

    int result = inflate(bit_reader.current_byte, compressed_length - 6, uncompressed, uncompressed_length);
    nextByte(&bit_reader);
    getBits(&bit_reader, 32);

    return result;
}
