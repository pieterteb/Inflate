#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>



uint16_t* huffmanTable(size_t* code_lengths, size_t code_count, size_t* table_size, size_t* max_code_length);



#endif /* HUFFMAN_H */
