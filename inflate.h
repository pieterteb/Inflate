/* https://datatracker.ietf.org/doc/html/rfc1951#section-3 */

#ifndef INFLATE_H
#define INFLATE_H


#include <stddef.h>

#include "MDE.h"

/* Inflate success and error codes. */
enum InflateError {
    INFLATE_SUCCESS = 0,
    INFLATE_NO_OUTPUT,
    INFLATE_NO_MEMORY,
    INFLATE_INVALID_BLOCK_TYPE,
    INFLATE_COMPRESSED_INCOMPLETE,
    INFLATE_DECOMPRESSED_OVERFLOW,
    INFLATE_BLOCK_LENGTH_UNCERTAIN,
    INFLATE_VALUE_NOT_ALLOWED,
    INFLATE_INVALID_LZ77
};


extern int tinflate(const unsigned char* compressed, size_t compressed_length, unsigned char* decompressed, size_t* decompressed_length, size_t decompressed_max_length);


#endif /* INFLATE_H */
