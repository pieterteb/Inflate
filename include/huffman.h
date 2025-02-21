#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>

#include "bit_reader.h"



/**
 * @brief Generates a deflate compliant Huffman table given an array of code lengths. The huffman table elements contain the value in the first 16 bits and the length of the code in the next 16 bits.
 * 
 * @param code_lengths Array of code lengths.
 * @param code_count Number of codes.
 * @return unsigned* 
 */
unsigned int* huffman_table(const size_t* code_lengths, const size_t code_count, size_t* max_code_length);

/**
 * @brief Consumes and returns the value corresponding to the next code in @a bit_reader. Returns INFLATE_GENERAL_FAILURE if @a bit_reader runs out of bits.
 * 
 * @param bit_reader Contains compressed data.
 * @param table Huffman table.
 * @param max_code_length Length of a longest code used to create @a table.
 * @return unsigned int
 */
unsigned int get_value(BitReader* bit_reader, const unsigned int* table, const size_t max_code_length);



#endif /* HUFFMAN_H */
