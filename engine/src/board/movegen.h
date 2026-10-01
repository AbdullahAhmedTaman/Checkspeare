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

    /**
     * @brief Appends a generated move to the list.
     * @param m Move to append; callers must keep the list below MAX_MOVES.
     */
    void add(Move m)
    {
        moves[count++] = m;
    }

    /**
     * @brief Reports whether the list contains no moves.
     * @return True when no moves have been generated.
     */
    bool empty() const
    {
        return count == 0;
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
    void generate_captures(const Position& pos, MoveList& list);
} // namespace MoveGen