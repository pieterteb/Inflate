#include "zlib_decompress.h"
#include "bit_reader.h"
#include "inflate.h"



extern int zlib_decompress(const unsigned char* compressed, size_t compressed_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length) {
    *decompressed_length = 0;

    if (!decompressed) {
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

    get_bits(&bit_reader, 16);

    int result = tinflate(bit_reader.current_byte, compressed_length - 6, decompressed, decompressed_length, decompressed_max_length);
    byte_align(&bit_reader);
    get_bits(&bit_reader, 32);

    return result;
}
