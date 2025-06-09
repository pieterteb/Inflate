#ifndef BIT_READER_H
#define BIT_READER_H


#include <stdint.h>



#define BITMASK(n)  ((1ULL << n) - 1)


typedef uint64_t Buffer;


#define CHECK_ENOUGH_BITS_IN_BUFFER(count)                          \
do {                                                                \
    if (buffer_count < count)                                       \
        return INFLATE_COMPRESSED_INCOMPLETE;                       \
} while (0)

/* Fills bit buffer to 56-63 bits or less if not enough bytes are left. */
#define FILL_BUFFER()                                               \
do {                                                                \
    if (compressed_end - compressed_next >= sizeof(Buffer)) {       \
        buffer |= *(Buffer*)compressed_next << buffer_count;        \
        compressed_next += (63 - buffer_count) >> 3;                \
        buffer_count |= 56;                                         \
    } else {                                                        \
        while (buffer_count < 56) {                                 \
            if (compressed_next == compressed_end)                  \
                break;                                              \
            buffer |= (Buffer)*compressed_next << buffer_count;     \
            ++compressed_next;                                      \
            buffer_count += 8;                                      \
        }                                                           \
    }                                                               \
} while (0)

#define CONSUME_BITS(count)                                         \
do {                                                                \
    buffer >>= count;                                               \
    buffer_count -= count;                                          \
} while (0)

#define PEEK_BITS(count)    (buffer & BITMASK(count))



#endif /* BIT_READER_H */
