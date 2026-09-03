#pragma once
#include "types.h"

namespace Eval
{
    extern const int PAWN_MG[64];
    extern const int PAWN_EG[64];

    extern const int KNIGHT_MG[64];
    extern const int KNIGHT_EG[64];

    extern const int BISHOP_MG[64];
    extern const int BISHOP_EG[64];

    extern const int ROOK_MG[64];
    extern const int ROOK_EG[64];

    extern const int QUEEN_MG[64];
    extern const int QUEEN_EG[64];

    extern const int KING_MG[64];
    extern const int KING_EG[64];

    // use this to flip the square index for black's perspective
    inline Square mirror_square(Square sq)
    {
        return static_cast<Square>(sq ^ 56);
    }
} // namespace Eval