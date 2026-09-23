#pragma once
#include "types.h"
#include <cstring>

namespace Search
{
    constexpr int MAX_PLY = 256;
    constexpr int MAX_PIECES = 13;
    constexpr int MAX_SQUARES = 64;
    constexpr int MAX_HISTORY_SCORE = 16384;

    extern Move killer_moves[MAX_PLY][2];
    extern int history_table[MAX_PIECES][MAX_SQUARES];

    /**
     * @brief Returns whether a move can be stored as a killer move.
     * @param move Candidate move to validate.
     * @return True only for a normal, non-capturing move.
     */
    inline bool is_killer_candidate(Move move)
    {
        return move.is_ok() && !move.is_null() && !move.is_capture() && !move.is_promotion();
    }

    /**
     * @brief Records a quiet move that caused a beta cutoff at a ply.
     *
     * The newest distinct move is kept first and the previous first move is
     * shifted into the second killer slot.
     */
    inline void update_killers(Move move, int ply)
    {
        if (ply < 0 || ply >= MAX_PLY || !is_killer_candidate(move))
            return;

        if (killer_moves[ply][0] != move)
        {
            killer_moves[ply][1] = killer_moves[ply][0];
            killer_moves[ply][0] = move;
        }
    }

    /**
     * @brief Reads the history value for a move.
     * @param move Move whose destination is used as the history key.
     * @param piece Moving piece used as the first history key.
     * @return Bounded history score, or zero for invalid input.
     */
    inline int history_score(Move move, Piece piece)
    {
        if (piece >= MAX_PIECES || !is_valid(move.to()))
            return 0;

        return history_table[piece][move.to()];
    }

    /**
     * @brief Rewards a move that contributed to a successful search cutoff.
     *
     * Scores are indexed by moving piece and destination square and are
     * capped to keep the heuristic bounded.
     */
    inline void update_history(Move move, int depth, Piece piece)
    {
        if (depth <= 0 || piece >= MAX_PIECES || piece == PIECE_NONE || !is_valid(move.to()))
            return;

        int& score = history_table[piece][move.to()];
        int bonus = depth > 128 ? MAX_HISTORY_SCORE : depth * depth;
        score += bonus;

        if (score > MAX_HISTORY_SCORE)
            score = MAX_HISTORY_SCORE;
    }

    /**
     * @brief Penalizes a quiet move that failed to improve alpha.
     * @param move Move whose history value should be reduced.
     * @param depth Search depth used to scale the penalty.
     * @param piece Moving piece used as the history key.
     */
    inline void penalize_history(Move move, int depth, Piece piece)
    {
        if (depth <= 0 || piece >= MAX_PIECES || piece == PIECE_NONE || !is_valid(move.to()))
            return;

        int& score = history_table[piece][move.to()];
        const int penalty = depth > 128 ? MAX_HISTORY_SCORE : depth * depth;
        score = score > penalty ? score - penalty : 0;
    }
} // namespace Search