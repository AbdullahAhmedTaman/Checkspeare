#pragma once
#include "board/types.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace Search
{
    constexpr int MAX_PIECES = 13;
    constexpr int MAX_SQUARES = 64;
    constexpr int MAX_HISTORY_SCORE = 16384;

    extern Move killer_moves[MAX_PLY][2];
    extern int history_table[MAX_PIECES][MAX_SQUARES];

    /** @brief Move played at each ply along the current search branch. */
    extern Move path_moves[MAX_PLY];

    /** @brief Moving piece at each ply along the current search branch. */
    extern Piece path_pieces[MAX_PLY];

    /** @brief Countermove table indexed by [prev_piece][prev_to]. */
    extern Move counter_moves[MAX_PIECES][MAX_SQUARES];

    /**
     * @brief Compact 1-ply Continuation History table indexed by:
     *        [prev_piece (0..12)][prev_to (0..63)][curr_piece_type (0..5)][curr_to (0..63)].
     *        Size: 13 * 64 * 6 * 64 * 2 bytes = 638 KB (fits in L2/L3 cache).
     */
    extern int16_t cont_history[MAX_PIECES][MAX_SQUARES][PieceType::NONE][MAX_SQUARES];

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
        score += bonus - ((score * bonus) / MAX_HISTORY_SCORE);
        score = std::clamp(score, -MAX_HISTORY_SCORE, MAX_HISTORY_SCORE);
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
        score -= penalty + ((score * penalty) / MAX_HISTORY_SCORE);
        score = std::clamp(score, -MAX_HISTORY_SCORE, MAX_HISTORY_SCORE);
    }

    inline void update_countermove(Piece prev_piece, Square prev_to, Move refutation) noexcept
    {
        if (prev_piece >= PIECE_NONE || !is_valid(prev_to) || !is_killer_candidate(refutation))
            return;

        counter_moves[prev_piece][prev_to] = refutation;
    }

    inline Move counter_move(Piece prev_piece, Square prev_to) noexcept
    {
        if (prev_piece >= PIECE_NONE || !is_valid(prev_to))
            return Move::none();

        return counter_moves[prev_piece][prev_to];
    }

    inline void update_continuation_history(Piece prev_piece, Square prev_to, Piece curr_piece, Square curr_to,
                                            int bonus) noexcept
    {
        const PieceType curr_pt = type_of(curr_piece);
        if (prev_piece >= PIECE_NONE || curr_pt >= PieceType::NONE || !is_valid(prev_to) || !is_valid(curr_to))
            return;

        const int clamped = std::clamp(bonus, -MAX_HISTORY_SCORE, MAX_HISTORY_SCORE);
        int entry = cont_history[prev_piece][prev_to][curr_pt][curr_to];
        entry += clamped - ((std::abs(entry) * clamped) / MAX_HISTORY_SCORE);
        cont_history[prev_piece][prev_to][curr_pt][curr_to] = static_cast<int16_t>(entry);
    }

    inline int continuation_history_score(Piece prev_piece, Square prev_to, Piece curr_piece, Square curr_to) noexcept
    {
        const PieceType curr_pt = type_of(curr_piece);
        if (prev_piece >= PIECE_NONE || curr_pt >= PieceType::NONE || !is_valid(prev_to) || !is_valid(curr_to))
            return 0;

        return cont_history[prev_piece][prev_to][curr_pt][curr_to];
    }

    /**
     * @brief Halves history scores at the start of a new turn to retain inter-turn learning.
     */
    inline void age_history() noexcept
    {
        for (int p = 0; p < MAX_PIECES; ++p)
        {
            for (int sq = 0; sq < MAX_SQUARES; ++sq)
            {
                history_table[p][sq] /= 2;
            }
        }
    }
} // namespace Search