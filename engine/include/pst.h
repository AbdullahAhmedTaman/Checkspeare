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

    extern const int* PST_MG[6];
    extern const int* PST_EG[6];

    // (Pawn=0, Knight=1, Bishop=1, Rook=2, Queen=4, King=0)
    constexpr int PHASE_WEIGHTS[6] = {0, 1, 1, 2, 4, 0};

    // use this to flip the square index for white's perspective
    inline Square mirror_square(Square sq)
    {
        return static_cast<Square>(sq ^ 56);
    }
} // namespace Eval