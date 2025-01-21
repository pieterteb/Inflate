#ifndef INFLATE_H
#define INFLATE_H


#define INFLATE_END_OF_COMPRESSED   -1


#include <stddef.h>



extern int inflate(const char* restrict compressed, const size_t compressed_length, char** const restrict uncompressed, size_t* const restrict uncompressed_length);



#endif /* INFLATE_H */
