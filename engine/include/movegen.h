#pragma once

#include "attacks.h"
#include "bitboard.h"
#include "position.h"
#include "types.h"

constexpr int MAX_MOVES = 256;

struct MoveList
{
    Move moves[MAX_MOVES];
    int count = 0;

    void add(Move m)
    {
        moves[count++] = m;
    }

    int size() const
    {
        return count;
    }

    Move* begin()
    {
        return moves;
    }

    Move* end()
    {
        return moves + count;
    }

    const Move* begin() const
    {
        return moves;
    }

    const Move* end() const
    {
        return moves + count;
    }
};

namespace MoveGen
{
    void generate_pseudo_legal_moves(const Position& pos, MoveList& list);
    void generate_legal_moves(Position& pos, MoveList& legalMoves);
} // namespace MoveGen