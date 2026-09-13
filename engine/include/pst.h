#pragma once

namespace Eval
{
    // (Pawn=0, Knight=1, Bishop=1, Rook=2, Queen=4, King=0)
    constexpr int PHASE_WEIGHTS[6] = {0, 1, 1, 2, 4, 0};

    struct PrecomputedPST
    {
        // [Color][PieceType][Square]
        int mg[2][6][64];
        int eg[2][6][64];
    };

    extern const PrecomputedPST PST;

    // // use this to flip the square index for white's perspective
    // inline Square mirror_square(Square sq)
    // {
    //     return static_cast<Square>(sq ^ 56);
    // }
} // namespace Eval