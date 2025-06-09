/* 
* This huffman decoding method is a slightly modified version of
* https://github.com/ebiggers/libdeflate/blob/master/lib/deflate_decompress.c
*/

#include "huffman.h"

#include <stdint.h>
#include <string.h>

#include "inflate.h"
#include "inflate_internal.h"



static uint32_t make_table_entry(const uint32_t decode[], uint32_t code, uint32_t length) {
    return decode[code] + (length << 8) + length;
}

/*
 * Format code length huffman table. Bits not described contain zeroes: 
 * 
 *      Bit 20-16: codeword
 *      Bit 10-8:  codeword length [not used]
 *      Bit 2-0:   codeword length (dynamically determined)
 * 
 * The code length huffman table does not contain subtables.
 */
static const uint32_t code_length_decode[] = {
#define ENTRY(codeword) ((uint32_t)codeword << 16)
        ENTRY(0),       ENTRY(1),       ENTRY(2),       ENTRY(3),
        ENTRY(4),       ENTRY(5),       ENTRY(6),       ENTRY(7),
        ENTRY(8),       ENTRY(9),       ENTRY(10),      ENTRY(11),
        ENTRY(12),      ENTRY(13),      ENTRY(14),      ENTRY(15),
        ENTRY(16),      ENTRY(17),      ENTRY(18),
#undef ENTRY
};


/* Indicates a literal entry in the literal table. */
#define HUFFMAN_LITERAL             0x80000000

/* Indicates that HUFFMAN_SUBTABLE_POINTER or HUFFMAN_END_OF_BLOCK */
#define HUFFMAN_EXCEPTIONAL         0x00008000

/* Indicates a subtable pointer entry in the literal or distance table. */
#define HUFFMAN_SUBTABLE_POINTER    0x00004000

/* Indicates end-of-block entry in the literal table. */
#define HUFFMAN_END_OF_BLOCK        0x00002000


/*
 * Here is the format of the literal table entries. Bits not explicitly
 * described contain zeroes:
 *
 *	Literals:
 *		Bit 31:     1 (HUFFMAN_LITERAL)
 *		Bit 23-16:  literal value
 *		Bit 15:     0 (!HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     0 (!HUFFMAN_SUBTABLE_POINTER)
 *		Bit 13:     0 (!HUFFMAN_END_OF_BLOCK)
 *		Bit 11-8:   remaining codeword length [not used]
 *		Bit 3-0:    remaining codeword length
 *	Lengths:
 *		Bit 31:     0 (!HUFFMAN_LITERAL)
 *		Bit 24-16:  length base value
 *		Bit 15:     0 (!HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     0 (!HUFFMAN_SUBTABLE_POINTER)
 *		Bit 13:     0 (!HUFFMAN_END_OF_BLOCK)
 *		Bit 11-8:   remaining codeword length
 *		Bit 4-0:    remaining codeword length + number of extra bits
 *	End of block:
 *		Bit 31:     0 (!HUFFMAN_LITERAL)
 *		Bit 15:     1 (HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     0 (!HUFFMAN_SUBTABLE_POINTER)
 *		Bit 13:     1 (HUFFMAN_END_OF_BLOCK)
 *		Bit 11-8:   remaining codeword length [not used]
 *		Bit 3-0:    remaining codeword length
 *	Subtable pointer:
 *		Bit 31:     0 (!HUFFMAN_LITERAL)
 *		Bit 30-16:  index of start of subtable
 *		Bit 15:     1 (HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     1 (HUFFMAN_SUBTABLE_POINTER)
 *		Bit 13:     0 (!HUFFMAN_END_OF_BLOCK)
 *		Bit 11-8:   number of subtable bits
 *		Bit 3-0:    number of main table bits
 */


static const uint32_t literal_decode[] = {

        /* Literals. */
#define ENTRY(literal)  (HUFFMAN_LITERAL | ((uint32_t)(literal) << 16))
        ENTRY(0),       ENTRY(1),       ENTRY(2),       ENTRY(3),
        ENTRY(4),       ENTRY(5),       ENTRY(6),       ENTRY(7),
        ENTRY(8),       ENTRY(9),       ENTRY(10),      ENTRY(11),
        ENTRY(12),      ENTRY(13),      ENTRY(14),      ENTRY(15),
        ENTRY(16),      ENTRY(17),      ENTRY(18),      ENTRY(19),
        ENTRY(20),      ENTRY(21),      ENTRY(22),      ENTRY(23),
        ENTRY(24),      ENTRY(25),      ENTRY(26),      ENTRY(27),
        ENTRY(28),      ENTRY(29),      ENTRY(30),      ENTRY(31),
        ENTRY(32),      ENTRY(33),      ENTRY(34),      ENTRY(35),
        ENTRY(36),      ENTRY(37),      ENTRY(38),      ENTRY(39),
        ENTRY(40),      ENTRY(41),      ENTRY(42),      ENTRY(43),
        ENTRY(44),      ENTRY(45),      ENTRY(46),      ENTRY(47),
        ENTRY(48),      ENTRY(49),      ENTRY(50),      ENTRY(51),
        ENTRY(52),      ENTRY(53),      ENTRY(54),      ENTRY(55),
        ENTRY(56),      ENTRY(57),      ENTRY(58),      ENTRY(59),
        ENTRY(60),      ENTRY(61),      ENTRY(62),      ENTRY(63),
        ENTRY(64),      ENTRY(65),      ENTRY(66),      ENTRY(67),
        ENTRY(68),      ENTRY(69),      ENTRY(70),      ENTRY(71),
        ENTRY(72),      ENTRY(73),      ENTRY(74),      ENTRY(75),
        ENTRY(76),      ENTRY(77),      ENTRY(78),      ENTRY(79),
        ENTRY(80),      ENTRY(81),      ENTRY(82),      ENTRY(83),
        ENTRY(84),      ENTRY(85),      ENTRY(86),      ENTRY(87),
        ENTRY(88),      ENTRY(89),      ENTRY(90),      ENTRY(91),
        ENTRY(92),      ENTRY(93),      ENTRY(94),      ENTRY(95),
        ENTRY(96),      ENTRY(97),      ENTRY(98),      ENTRY(99),
        ENTRY(100),     ENTRY(101),     ENTRY(102),     ENTRY(103),
        ENTRY(104),     ENTRY(105),     ENTRY(106),     ENTRY(107),
        ENTRY(108),     ENTRY(109),     ENTRY(110),     ENTRY(111),
        ENTRY(112),     ENTRY(113),     ENTRY(114),     ENTRY(115),
        ENTRY(116),     ENTRY(117),     ENTRY(118),     ENTRY(119),
        ENTRY(120),     ENTRY(121),     ENTRY(122),     ENTRY(123),
        ENTRY(124),     ENTRY(125),     ENTRY(126),     ENTRY(127),
        ENTRY(128),     ENTRY(129),     ENTRY(130),     ENTRY(131),
        ENTRY(132),     ENTRY(133),     ENTRY(134),     ENTRY(135),
        ENTRY(136),     ENTRY(137),     ENTRY(138),     ENTRY(139),
        ENTRY(140),     ENTRY(141),     ENTRY(142),     ENTRY(143),
        ENTRY(144),     ENTRY(145),     ENTRY(146),     ENTRY(147),
        ENTRY(148),     ENTRY(149),     ENTRY(150),     ENTRY(151),
        ENTRY(152),     ENTRY(153),     ENTRY(154),     ENTRY(155),
        ENTRY(156),     ENTRY(157),     ENTRY(158),     ENTRY(159),
        ENTRY(160),     ENTRY(161),     ENTRY(162),     ENTRY(163),
        ENTRY(164),     ENTRY(165),     ENTRY(166),     ENTRY(167),
        ENTRY(168),     ENTRY(169),     ENTRY(170),     ENTRY(171),
        ENTRY(172),     ENTRY(173),     ENTRY(174),     ENTRY(175),
        ENTRY(176),     ENTRY(177),     ENTRY(178),     ENTRY(179),
        ENTRY(180),     ENTRY(181),     ENTRY(182),     ENTRY(183),
        ENTRY(184),     ENTRY(185),     ENTRY(186),     ENTRY(187),
        ENTRY(188),     ENTRY(189),     ENTRY(190),     ENTRY(191),
        ENTRY(192),     ENTRY(193),     ENTRY(194),     ENTRY(195),
        ENTRY(196),     ENTRY(197),     ENTRY(198),     ENTRY(199),
        ENTRY(200),     ENTRY(201),     ENTRY(202),     ENTRY(203),
        ENTRY(204),     ENTRY(205),     ENTRY(206),     ENTRY(207),
        ENTRY(208),     ENTRY(209),     ENTRY(210),     ENTRY(211),
        ENTRY(212),     ENTRY(213),     ENTRY(214),     ENTRY(215),
        ENTRY(216),     ENTRY(217),     ENTRY(218),     ENTRY(219),
        ENTRY(220),     ENTRY(221),     ENTRY(222),     ENTRY(223),
        ENTRY(224),     ENTRY(225),     ENTRY(226),     ENTRY(227),
        ENTRY(228),     ENTRY(229),     ENTRY(230),     ENTRY(231),
        ENTRY(232),     ENTRY(233),     ENTRY(234),     ENTRY(235),
        ENTRY(236),     ENTRY(237),     ENTRY(238),     ENTRY(239),
        ENTRY(240),     ENTRY(241),     ENTRY(242),     ENTRY(243),
        ENTRY(244),     ENTRY(245),     ENTRY(246),     ENTRY(247),
        ENTRY(248),     ENTRY(249),     ENTRY(250),     ENTRY(251),
        ENTRY(252),     ENTRY(253),     ENTRY(254),     ENTRY(255),
#undef ENTRY

        /* End of block. */
        HUFFMAN_EXCEPTIONAL | HUFFMAN_END_OF_BLOCK,

        /* Lengths. */
#define ENTRY(length_base, length_extra_bits)   (((uint32_t)(length_base) << 16) | (length_extra_bits))
        ENTRY(3, 0),    ENTRY(4, 0),    ENTRY(5, 0),    ENTRY(6, 0),
        ENTRY(7, 0),    ENTRY(8, 0),    ENTRY(9, 0),    ENTRY(10, 0),
        ENTRY(11, 1),   ENTRY(13, 1),   ENTRY(15, 1),   ENTRY(17, 1),
        ENTRY(19, 2),   ENTRY(23, 2),   ENTRY(27, 2),   ENTRY(31, 2),
        ENTRY(35, 3),   ENTRY(43, 3),   ENTRY(51, 3),   ENTRY(59, 3),
        ENTRY(67, 4),   ENTRY(83, 4),   ENTRY(99, 4),   ENTRY(115, 4),
        ENTRY(131, 5),  ENTRY(163, 5),  ENTRY(195, 5),  ENTRY(227, 5),
        ENTRY(258, 0),  ENTRY(258, 0),  ENTRY(258, 0),
};


/*
 * Here is the format of the distance table entries. Bits not explicitly
 * described contain zeroes:
 *
 *	Offsets:
 *		Bit 31-16:  distance base value
 *		Bit 15:     0 (!HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     0 (!HUFFMAN_SUBTABLE_POINTER)
 *		Bit 11-8:   remaining codeword length
 *		Bit 4-0:    remaining codeword length + number of extra bits
 *	Subtable pointer:
 *		Bit 31-16:  index of start of subtable
 *		Bit 15:     1 (HUFFMAN_EXCEPTIONAL)
 *		Bit 14:     1 (HUFFMAN_SUBTABLE_POINTER)
 *		Bit 11-8:   number of subtable bits
 *		Bit 3-0:    number of main table bits
 *
 * These work the same way as the length entries and subtable pointer entries in
 * the literal table.
 */


static const uint32_t distance_decode[] = {
#define ENTRY(distance_base, distance_extra_bits)   (((uint32_t)(distance_base) << 16) | (distance_extra_bits))
        ENTRY(1, 0),        ENTRY(2, 0),        ENTRY(3, 0),        ENTRY(4, 0),
        ENTRY(5, 1),        ENTRY(7, 1),        ENTRY(9, 2),        ENTRY(12, 2),
        ENTRY(17, 3),       ENTRY(25, 3),       ENTRY(33, 4),       ENTRY(49, 4),
        ENTRY(65, 5),       ENTRY(97, 5),       ENTRY(129, 6),      ENTRY(193, 6),
        ENTRY(257, 7),      ENTRY(385, 7),      ENTRY(513, 8),      ENTRY(769, 8),
        ENTRY(1025, 9),     ENTRY(1537, 9),     ENTRY(2049, 10),    ENTRY(3073, 10),
        ENTRY(4097, 11),    ENTRY(6145, 11),    ENTRY(8193, 12),    ENTRY(12289, 12),
        ENTRY(16385, 13),   ENTRY(24577, 13),   ENTRY(24577, 13),   ENTRY(24577, 13),
#undef ENTRY
};


static int build_huffman_table(uint32_t table[], const uint8_t code_lengths[], const unsigned code_count, const uint32_t decode[], unsigned table_bits, unsigned max_code_length, uint16_t* sorted_codes, unsigned* table_bits_return) {
    /* Compute frequency of code lengths. */
    unsigned code_length_frequency[INFLATE_MAX_CODE_LENGTH + 1];
    for (unsigned i = 0; i <= max_code_length; ++i)
        code_length_frequency[i] = 0;
    for (unsigned i = 0; i < code_count; ++i)
        ++code_length_frequency[code_lengths[i]];
    
    /* Determine maximum code length that was used. */
    while (max_code_length > 1 && !code_length_frequency[max_code_length])
        --max_code_length;
    if (table_bits_return != NULL) {
        table_bits = table_bits < max_code_length ? table_bits : max_code_length;
        *table_bits_return = table_bits;
    }

    /* Sort codes by increasing code length and increasing code value. */
    unsigned offsets[INFLATE_MAX_CODE_LENGTH + 1];
    offsets[0] = 0;
    offsets[1] = code_length_frequency[0];
    uint32_t codespace_used = 0;
    for (unsigned i = 1; i < max_code_length; ++i) {
        offsets[i + 1] = offsets[i] + code_length_frequency[i];
        codespace_used = (codespace_used << 1) + code_length_frequency[i];
    }
    codespace_used = (codespace_used << 1) + code_length_frequency[max_code_length];

    for (unsigned i = 0; i < code_count; ++i) {
        sorted_codes[offsets[code_lengths[i]]] = i;
        ++offsets[code_lengths[i]];
    }
    sorted_codes += offsets[0]; /* Skip unused codes. */

    /* Overfull table. */
    if (codespace_used > (1U << max_code_length))
        return INFLATE_OVERFULL_HUFFMAN_CODE;

    /* Incomplete table. */
    if (codespace_used < (1U << max_code_length)) {
        unsigned code;
        if (!codespace_used) {
            code = 0;
        } else {
            if (codespace_used != (1U << (max_code_length - 1)) || code_length_frequency[1] != 1)
                return INFLATE_INCOMPLETE_HUFFMAN_CODE;
            code = sorted_codes[0];
        }
        uint32_t entry = make_table_entry(decode, code, 1);
        for (unsigned i = 0; i < (1U << table_bits); ++i)
            table[i] = entry;
        return INFLATE_SUCCESS;
    }

    /* Process all codes with length <= table_bits. */
    unsigned code = 0;
    unsigned length = 1;
    unsigned frequency = code_length_frequency[length];
    while (!frequency) {
        ++length;
        frequency = code_length_frequency[length];
    }
    unsigned current_table_end = 1U << length;
    while (length <= table_bits) {
        do {
            table[code] = make_table_entry(decode, *sorted_codes, length);
            ++sorted_codes;

            if (code == current_table_end - 1) {
                for (; length < table_bits; ++length) {
                    memcpy(&table[current_table_end], table, current_table_end * sizeof(table[0]));
                    current_table_end <<= 1;
                }
                return INFLATE_SUCCESS;
            }

            unsigned bit_scan_reversed = (code ^ (current_table_end - 1)) >> 1;
            unsigned i = 0;
            while (bit_scan_reversed) {
                ++i;
                bit_scan_reversed >>= 1;
            }

            unsigned bit = 1U << i;
            code &= bit - 1;
            code |= bit;

            --frequency;
        } while (frequency);

        do {
            ++length;
            if (length <= table_bits) {
                memcpy(&table[current_table_end], table, current_table_end * sizeof(table[0]));
                current_table_end <<= 1;
            }
            frequency = code_length_frequency[length];
        } while (!frequency);
    }

    /* Process codes with length > table_bits. These require subtables. */
    current_table_end = 1U << table_bits;
    unsigned subtable_prefix = -1;
    unsigned subtable_start = 0;
    for (;;) {
        if ((code & ((1U << table_bits) - 1)) != subtable_prefix) {
            subtable_prefix = code & ((1U << table_bits) - 1);
            subtable_start = current_table_end;
            
            unsigned subtable_bits = length - table_bits;
            codespace_used = frequency;
            while (codespace_used < (1U << subtable_bits)) {
                ++subtable_bits;
                codespace_used = (codespace_used << 1) + code_length_frequency[table_bits + subtable_bits];
            }
            current_table_end = subtable_start + (1U << subtable_bits);

            table[subtable_prefix] = ((uint32_t)subtable_start << 16) | HUFFMAN_EXCEPTIONAL | HUFFMAN_SUBTABLE_POINTER | (subtable_bits << 8) | table_bits;
        }

        uint32_t entry = make_table_entry(decode, *sorted_codes, length - table_bits);
        ++sorted_codes;
        unsigned i = subtable_start + (code >> table_bits);
        unsigned stride = 1U << (length - table_bits);
        do {
            table[i] = entry;
            i += stride;
        } while (i < current_table_end);

        if (code == (1U << length) - 1)
            return INFLATE_SUCCESS;

        unsigned bit_scan_reversed = (code ^ (current_table_end - 1)) >> 1;
        unsigned i = 0;
        while (bit_scan_reversed) {
            ++i;
            bit_scan_reversed >>= 1;
        }

        unsigned bit = 1U << i;
        code &= bit - 1;
        code |= bit;

        --frequency;
        while (!frequency) {
            ++length;
            frequency = code_length_frequency[length];
        }
    }
}

int build_code_length_table(struct Inflator* inflator) {
    return build_huffman_table(inflator->u.s.code_length_table, inflator->u.code_length_code_lengths, INFLATE_CODE_LENGTH_CODE_COUNT, code_length_decode, CODE_LENGTH_TABLE_BITS, INFLATE_MAX_CODE_LENGTH_CODE_LENGTH, inflator->sorted_codes, NULL);
}

int build_literal_table(struct Inflator* inflator, unsigned literal_code_count) {
    return build_huffman_table(inflator->u.literal_table, inflator->u.s.code_lengths, literal_code_count, literal_decode, LITERAL_TABLE_BITS, INFLATE_MAX_LITERAL_CODE_LENGTH, inflator->sorted_codes, &inflator->literal_table_bits);
}

int build_distance_table(struct Inflator* inflator, unsigned literal_code_count, unsigned distance_code_count) {
    return build_huffman_table(inflator->distance_table, inflator->u.s.code_lengths + literal_code_count, distance_code_count, distance_decode, DISTANCE_TABLE_BITS, INFLATE_MAX_DISTANCE_CODE_LENGTH, inflator->sorted_codes, NULL);
}
