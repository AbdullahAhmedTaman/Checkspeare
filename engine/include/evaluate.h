#pragma once
#include "position.h"
#include "types.h"

struct Score
{
    int mg = 0;
    int eg = 0;

    Score() = default;

    Score(int m, int e)
        : mg(m),
          eg(e)
    {
    }

    Score& operator+=(const Score& rhs)
    {
        mg += rhs.mg;
        eg += rhs.eg;
        return *this;
    }

    Score& operator-=(const Score& rhs)
    {
        mg -= rhs.mg;
        eg -= rhs.eg;
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