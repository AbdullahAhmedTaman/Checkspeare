#pragma once

#include <cassert>

#include "types.h"

// Bitboard constants representing empty and full boards.
constexpr Bitboard BB_EMPTY = 0ULL;
constexpr Bitboard BB_FULL = ~Bitboard{0};

// Precomputed bitmasks for individual board files (A-H).
constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_B = FILE_A << 1;
constexpr Bitboard FILE_C = FILE_A << 2;
constexpr Bitboard FILE_D = FILE_A << 3;
constexpr Bitboard FILE_E = FILE_A << 4;
constexpr Bitboard FILE_F = FILE_A << 5;
constexpr Bitboard FILE_G = FILE_A << 6;
constexpr Bitboard FILE_H = FILE_A << 7;

// Precomputed bitmasks for individual board ranks (1-8).
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_2 = RANK_1 << (8 * 1);
constexpr Bitboard RANK_3 = RANK_1 << (8 * 2);
constexpr Bitboard RANK_4 = RANK_1 << (8 * 3);
constexpr Bitboard RANK_5 = RANK_1 << (8 * 4);
constexpr Bitboard RANK_6 = RANK_1 << (8 * 5);
constexpr Bitboard RANK_7 = RANK_1 << (8 * 6);
constexpr Bitboard RANK_8 = RANK_1 << (8 * 7);

// clang-format off

// Precomputed occupancy masks for all 15 main diagonals (A1-H8 direction).
// Indexed by (rank - file + 7), resulting in a range of 0..14.
inline constexpr Bitboard DIAGONAL_BB[15] =
{
    0x0000000000000080ULL,
    0x0000000000008040ULL,
    0x0000000000804020ULL,
    0x0000000080402010ULL,
    0x0000008040201008ULL,
    0x0000804020100804ULL,
    0x0080402010080402ULL,
    0x8040201008040201ULL,
    0x4020100804020100ULL,
    0x2010080402010000ULL,
    0x1008040201000000ULL,
    0x0804020100000000ULL,
    0x0402010000000000ULL,
    0x0201000000000000ULL,
    0x0100000000000000ULL
};

// Precomputed occupancy masks for all 15 anti-diagonals (A8-H1 direction).
// Indexed by (rank + file), resulting in a range of 0..14.
inline constexpr Bitboard ANTI_DIAGONAL_BB[15] =
{
    0x0000000000000001ULL,
    0x0000000000000102ULL,
    0x0000000000010204ULL,
    0x0000000001020408ULL, 
    0x0000000102040810ULL, 
    0x0000010204081020ULL, 
    0x0001020408102040ULL, 
    0x0102040810204080ULL,
    0x0204081020408000ULL, 
    0x0408102040800000ULL, 
    0x0810204080000000ULL, 
    0x1020408000000000ULL, 
    0x2040800000000000ULL,
    0x4080000000000000ULL, 
    0x8000000000000000ULL  
};

// clang-format on

// Returns a single-bit bitboard for the specified square.
constexpr Bitboard square_bb(Square square)
{
    return 1ULL << square;
}

// Checks if a given square is set (occupied) in the bitboard.
constexpr bool get_bit(Bitboard bb, Square square)
{
    return (bb & square_bb(square)) != 0;
}

// Sets the bit corresponding to the specified square.
constexpr void set_bit(Bitboard& bb, Square square)
{
    bb |= square_bb(square);
}

// Clears the bit corresponding to the specified square.
constexpr void clear_bit(Bitboard& bb, Square square)
{
    bb &= ~square_bb(square);
}

// Toggles the bit corresponding to the specified square.
constexpr void toggle_bit(Bitboard& bb, Square square)
{
    bb ^= square_bb(square);
}

// Returns the total number of set bits (population count).
inline int popcount(Bitboard bb)
{
    return __builtin_popcountll(bb);
}

// Returns the square index of the least significant set bit (LSB).
// Precondition: bb != 0.
inline Square lsb(Bitboard bb)
{
    assert(bb != 0);
    return static_cast<Square>(__builtin_ctzll(bb));
}

// Returns the LSB index and clears it from the bitboard in-place.
// Precondition: bb != 0.
inline Square pop_lsb(Bitboard& bb)
{
    Square square = lsb(bb);
    bb &= bb - 1;
    return square;
}

// Extracts the zero-based rank (0 for rank 1, 7 for rank 8) from a square.
constexpr int rank_of(Square square)
{
    return square >> 3;
}

// Extracts the zero-based file (0 for file A, 7 for file H) from a square.
constexpr int file_of(Square square)
{
    return square & 7;
}

// Returns a bitboard mask containing all squares on the same rank as the given square.
constexpr Bitboard rank_bb(Square square)
{
    return 0xFFULL << (square & 56);
}

// Returns a bitboard mask containing all squares on the same file as the given square.
constexpr Bitboard file_bb(Square square)
{
    return 0x0101010101010101ULL << file_of(square);
}

// Returns a bitboard mask for the main diagonal (A1-H8 direction) passing through the square.
constexpr Bitboard diagonal_bb(Square square)
{
    return DIAGONAL_BB[rank_of(square) - file_of(square) + 7];
}

// Returns a bitboard mask for the anti-diagonal (A8-H1 direction) passing through the square.
constexpr Bitboard anti_diagonal_bb(Square square)
{
    return ANTI_DIAGONAL_BB[rank_of(square) + file_of(square)];
}

constexpr Bitboard shift_north(Bitboard bb)

{
    return bb << 8;
}

constexpr Bitboard shift_south(Bitboard bb)

{
    return bb >> 8;
}

constexpr Bitboard shift_east(Bitboard b)
{
    return (b & ~FILE_H) << 1;
}

constexpr Bitboard shift_west(Bitboard b)
{
    return (b & ~FILE_A) >> 1;
}

constexpr Bitboard shift_north_east(Bitboard b)
{
    return (b & ~FILE_H) << 9;
}

constexpr Bitboard shift_north_west(Bitboard b)
{
    return (b & ~FILE_A) << 7;
}

constexpr Bitboard shift_south_east(Bitboard b)
{
    return (b & ~FILE_H) >> 7;
}

constexpr Bitboard shift_south_west(Bitboard b)
{
    return (b & ~FILE_A) >> 9;
}

constexpr Bitboard reverse_bb(Bitboard x)
{
    // Reverse bits within each byte (3 SWAR passes, byte-parallel).
    x = ((x & 0x5555555555555555ULL) << 1) | ((x >> 1) & 0x5555555555555555ULL);
    x = ((x & 0x3333333333333333ULL) << 2) | ((x >> 2) & 0x3333333333333333ULL);
    x = ((x & 0x0F0F0F0F0F0F0F0FULL) << 4) | ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL);

    // Reverse byte order — GCC lowers this to a single BSWAP instruction.
    return __builtin_bswap64(x);
}