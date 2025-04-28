/* https://datatracker.ietf.org/doc/html/rfc1950#section-3 */

#ifndef ZLIB_DECOMPRESS_H
#define ZLIB_DECOMPRESS_H

#include <stddef.h>

#include "MDE.h"



enum ZlibDecompressError {
    ZLIB_DECOMPRESS_SUCCESS = 0,
    ZLIB_DECOMPRESS_NO_OUTPUT,
};


extern int zlib_decompress(const unsigned char* compressed, size_t compressed_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);



#endif /* ZLIB_DECOMPRESS_H */
