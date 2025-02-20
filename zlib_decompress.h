/* https://datatracker.ietf.org/doc/html/rfc1950#section-3 */

#ifndef ZLIB_DECOMPRESS_H
#define ZLIB_DECOMPRESS_H

#include <stddef.h>



/* 
If not defined, zlib_decompress() is optimised with the assumption that input is error-free zlib.
If defined, invalid zlib will be handled by returning an error code, corresponding to the fault encountered first.
*/
#define ZLIB_DECOMPRESS_CAREFUL
#if defined(ZLIB_DECOMPRESS_CAREFUL) && !defined(INFLATE_CAREFUL)
#   define INFLATE_CAREFUL
#endif /* defined(ZLIB_DECOMPRESS_CAREFUL) && !defined(INFLATE_CAREFUL) */


enum {
    ZLIB_DECOMPRESS_SUCCESS = 0,
    ZLIB_DECOMPRESS_NO_OUTPUT,
};


extern int zlib_decompress(const unsigned char* compressed, size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length);



#endif /* ZLIB_DECOMPRESS_H */
