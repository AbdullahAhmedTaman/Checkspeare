#pragma once

#include "bitboard.h"
#include "types.h"

// Initialization
void init_attacks(); // Must be called once at the start of the engine

// Sliding pieces
Bitboard pawn_attacks(Color color, Square sq);

Bitboard knight_attacks(Square sq);

Bitboard king_attacks(Square sq);

// Non-sliding pieces
Bitboard bishop_attacks(Square sq, Bitboard occupancy);

Bitboard rook_attacks(Square sq, Bitboard occupancy);

Bitboard queen_attacks(Square sq, Bitboard occupancy);
