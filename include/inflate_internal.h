#ifndef INFLATE_INTERNAL_H
#define INFLATE_INTERNAL_H



#define INFLATE_GENERAL_FAILURE             (unsigned int)-1


/* Deflate defined constants (RFC 1951). */

/* Block types. */
#define INFLATE_BLOCKTYPE_UNCOMPRESSED      0
#define INFLATE_BLOCKTYPE_STATIC_HUFFMAN    1
#define INFLATE_BLOCKTYPE_DYNAMIC_HUFFMAN   2

/* lz77 */
#define INFLATE_MIN_LZ77_LENGTH             3
#define INFLATE_MAX_LZ77_LENGTH             258
#define INFLATE_MAX_LZ77_DISTANCE           32768

/* Code counts. */
#define INFLATE_CODE_LENGTH_CODE_COUNT      19
#define INFLATE_LITERAL_CODE_COUNT          288
#define INFLATE_DISTANCE_CODE_COUNT         32

#define INFLATE_MAX_CODE_COUNT              288

/* Code division. */
#define INFLATE_END_OF_BLOCK                256

/* Maximum codeword length. */
#define INFLATE_MAX_CODE_LENGTH_CODE_LENGTH 7
#define INFLATE_MAX_LITERAL_CODE_LENGTH     15
#define INFLATE_MAX_DISTANCE_CODE_LENGTH    15

#define INFLATE_MAX_CODE_LENGTH             15

/* Maximum possible overrun when decoding codeword lengths. */
#define INFLATE_MAX_LENGTHS_OVERRUN         137



#endif /* INFLATE_INTERNAL_H */
