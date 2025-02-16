/*
https://datatracker.ietf.org/doc/html/rfc1951#section-7
*/
#ifndef INFLATE_H
#define INFLATE_H


#include <stddef.h>



/* 
If not defined, inflate() is optimised with the assumption that input is error-free deflate. 
If defined, invalid deflate will be handled by returning an error code, corresponding to the fault encountered first.
*/
#define INFLATE_CAREFUL


/* Inflate success and error codes. */
enum {
    INFLATE_SUCCESS,
    INFLATE_NO_OUTPUT,
    INFLATE_NO_MEMORY,
    INFLATE_COMPRESSED_INCOMPLETE,
    INFLATE_INVALID_BLOCK_TYPE,
    INFLATE_WRONG_BLOCK_LENGTH,
};


extern int inflate(const unsigned char* compressed, const size_t compressed_length, unsigned char** uncompressed, size_t* uncompressed_length);



#endif /* INFLATE_H */
