#pragma once

#include "types.h"

// Initialization
void init_attacks(); // Must be called once at the start of the engine

// Non-liding pieces
Bitboard pawn_attacks(Color color, Square sq);

Bitboard knight_attacks(Square sq);

Bitboard king_attacks(Square sq);

// Sliding pieces
Bitboard bishop_attacks(Bitboard occupancy, Square sq);

Bitboard rook_attacks(Bitboard occupancy, Square sq);

Bitboard queen_attacks(Bitboard occupancy, Square sq);
