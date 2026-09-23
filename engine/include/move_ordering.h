#pragma once
#include "movegen.h"
#include "position.h"
#include "types.h"

namespace MoveOrdering
{
    constexpr int TT_MOVE_SCORE = 1000000;
    constexpr int CAPTURE_BONUS = 100000;
    constexpr int KILLER_SCORE = 90000;

    // Values: Pawn=0, Knight=1, Bishop=2, Rook=3, Queen=4, King=5
    // clang-format off
 const int MVV_LVA[PieceType::NONE][PieceType::NONE] = {
        {15, 14, 13, 12, 11, 10}, 
        {25, 24, 23, 22, 21, 20}, 
        {35, 34, 33, 32, 31, 30}, 
        {45, 44, 43, 42, 41, 40}, 
        {55, 54, 53, 52, 51, 50}, 
        {0, 0, 0, 0, 0, 0}      
    };
    // clang-format on

    class MovePicker
    {
    public:
        /**
         * @brief Copies and scores moves for best-first selection.
         * @param list Candidate moves to order.
         * @param pos Position used for capture scoring.
         * @param tt_move Previously stored transposition-table move.
         * @param ply Search ply used by search heuristics.
         */
        MovePicker(const MoveList& list, const Position& pos, Move tt_move = Move::none(), int ply = 0);

        /**
         * @brief Returns the highest-scoring remaining move.
         * @return The next move, or Move::none() when exhausted.
         */
        Move next_move();

    private:
        Move moves[MAX_MOVES];
        int scores[MAX_MOVES];
        int count;
        int current_index;

        /** @brief Assigns ordering scores to the copied move list. */
        void score_moves(const Position& pos, Move tt_move);
    };
} // namespace MoveOrdering