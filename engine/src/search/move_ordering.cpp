#include "move_ordering.h"
#include "heuristics.h"
#include <algorithm>
#include <iterator>

namespace MoveOrdering
{
    namespace
    {
        /**
         * @brief Calculates the ordering score for one move.
         * @param pos Position used to identify the captured and attacking pieces.
         * @param move Move to score.
         * @param tt_move Transposition-table move receiving highest priority.
         * @return Ordering score used by MovePicker.
         */
        int score_move(const Position& pos, Move move, Move tt_move, int ply)
        {
            if (move == tt_move)
                return TT_MOVE_SCORE;

            const int promotion_score = move.is_promotion() ? PROMOTION_BONUS + static_cast<int>(promotion_type(move)) * 1000 : 0;
            const Piece attacker = pos.board[move.from()];

            if (move.is_capture())
            {
                const PieceType attacker_type = type_of(attacker);
                const PieceType victim_type = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);

                if (attacker_type == NONE || victim_type == NONE)
                    return promotion_score;

                return CAPTURE_BONUS + MVV_LVA[victim_type][attacker_type] + promotion_score;
            }

            const bool is_killer = ply >= 0 && ply < Search::MAX_PLY &&
                                   (Search::killer_moves[ply][0] == move || Search::killer_moves[ply][1] == move);
            return (is_killer ? KILLER_SCORE : 0) + Search::history_score(move, attacker);
        }
    } // namespace

    bool MovePicker::lower_priority(const ScoredMove& left, const ScoredMove& right)
    {
        return left.score < right.score;
    }

    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move, int ply)
        : count(list.size()),
          ply(ply)
    {
        std::transform(list.begin(), list.end(), std::begin(moves),
                       [](Move move)
                       {
                           return ScoredMove{move, 0};
                       });
        score_moves(pos, tt_move);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        std::transform(std::begin(moves), std::begin(moves) + count, std::begin(moves),
                       [this, &pos, tt_move](const ScoredMove& scored_move)
                       {
                           return ScoredMove{scored_move.move, score_move(pos, scored_move.move, tt_move, ply)};
                       });
        std::make_heap(std::begin(moves), std::begin(moves) + count, lower_priority);
    }

    Move MovePicker::next_move()
    {
        if (count == 0)
        {
            return Move{};
        }

        std::pop_heap(std::begin(moves), std::begin(moves) + count, lower_priority);
        return moves[--count].move;
    }
} // namespace MoveOrdering