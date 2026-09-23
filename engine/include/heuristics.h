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

    /**
     * @brief Records a quiet move that caused a beta cutoff at a ply.
     *
     * The newest distinct move is kept first and the previous first move is
     * shifted into the second killer slot.
     */
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

    /**
     * @brief Rewards a move that contributed to a successful search cutoff.
     *
     * Scores are indexed by moving piece and destination square and are
     * capped to keep the heuristic bounded.
     */
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