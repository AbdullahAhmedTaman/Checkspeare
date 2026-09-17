#pragma once
#include "types.h"
#include <cstring>

namespace Search
{
    constexpr int MAX_PLY = 128;
    constexpr int MAX_PIECES = 13;
    constexpr int MAX_SQUARES = 64;
    constexpr int MAX_HISTORY_SCORE = 16384;

    extern Move killer_moves[MAX_PLY][2]; // saves best 2 killer moves got the best beta cuttoff
    extern int history_table[MAX_PIECES][MAX_SQUARES];

    inline void update_killers(Move move, int ply)
    {
        if (ply < 0 || ply >= MAX_PLY || !move.is_ok())
            return;

        if (killer_moves[ply][0] != move)
        {
            killer_moves[ply][1] = killer_moves[ply][0];
            killer_moves[ply][0] = move;
        }
    }

    inline void update_history(Move move, int depth, Piece piece)
    {
        if (depth <= 0 || piece >= MAX_PIECES || !is_valid(move.to()))
            return;

        int& score = history_table[piece][move.to()];
        int bonus = depth > 128 ? MAX_HISTORY_SCORE : depth * depth;
        score += bonus;

        if (score > MAX_HISTORY_SCORE)
            score = MAX_HISTORY_SCORE;
    }
} // namespace Search