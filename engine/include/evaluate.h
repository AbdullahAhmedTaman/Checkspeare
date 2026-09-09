#pragma once
#include "position.h"
#include "types.h"

struct Score
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};

    Score() = default;

    Score& operator+=(const Score& rhs)
    {
        mg[WHITE] += rhs.mg[WHITE];
        mg[BLACK] += rhs.mg[BLACK];
        eg[WHITE] += rhs.eg[WHITE];
        eg[BLACK] += rhs.eg[BLACK];
        return *this;
    }

    Score& operator-=(const Score& rhs)
    {
        mg[WHITE] -= rhs.mg[WHITE];
        mg[BLACK] -= rhs.mg[BLACK];
        eg[WHITE] -= rhs.eg[WHITE];
        eg[BLACK] -= rhs.eg[BLACK];
        return *this;
    }
};

namespace Eval
{ // material values =  {pawn, knight, bishop, rook, queen, king}
    constexpr int MG_WEIGHT[PieceType::NONE] = {82, 337, 365, 477, 1025, 0};
    constexpr int EG_WEIGHT[PieceType::NONE] = {94, 281, 297, 512, 936, 0};

    void init_scores(Position& pos); // call it once after parsing FEN
    int evaluate(const Position& pos);
    Score evaluate_bishop_pair(const Position& pos);
} // namespace Eval