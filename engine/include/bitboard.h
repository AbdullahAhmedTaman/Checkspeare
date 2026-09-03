#pragma once

#include "types.h"

constexpr Bitboard BB_EMPTY = 0ULL;
constexpr Bitboard BB_FULL = ~0ULL;

inline Bitboard square_bb(Square square)
{
    return 1ULL << square;
}

inline bool get_bit(Bitboard bb, Square square)
{
    return bb & square_bb(square);
}

inline void set_bit(Bitboard& bb, Square square)
{
    bb |= square_bb(square);
}

inline void clear_bit(Bitboard& bb, Square square)
{
    bb &= ~square_bb(square);
}

inline void toggle_bit(Bitboard& bb, Square square)
{
    bb ^= square_bb(square);
}

inline int popcount(Bitboard bb)
{
    return __builtin_popcountll(bb);
}

inline Square lsb(Bitboard bb)
{
    return static_cast<Square>(__builtin_ctzll(bb));
}

inline Square pop_lsb(Bitboard& bb)
{
    Square s = lsb(bb);
    bb &= bb - 1;
    return s;
}