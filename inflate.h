/*
https://datatracker.ietf.org/doc/html/rfc1951#section-7
*/

#ifndef INFLATE_H
#define INFLATE_H


#define INFLATE_END_OF_COMPRESSED   -1


#include <stddef.h>



extern int inflate(const unsigned char* restrict compressed, const size_t compressed_length, unsigned char** const restrict uncompressed, size_t* const restrict uncompressed_length);



#endif /* INFLATE_H */
