#include "move_ordering.h"
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
        int score_move(const Position& pos, Move move, Move tt_move)
        {
            if (move == tt_move)
                return TT_MOVE_SCORE;

            if (!move.is_capture())
                return 0;

            const Piece attacker = pos.board[move.from()];
            const PieceType attacker_type = type_of(attacker);
            const PieceType victim_type = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);

            if (attacker_type == NONE || victim_type == NONE)
                return 0;

            return CAPTURE_BONUS + MVV_LVA[victim_type][attacker_type];
        }
    } // namespace

    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move, int ply)
        : count(list.size()),
          current_index(0),
          ply(ply)
    {
        std::copy(list.begin(), list.end(), std::begin(moves));
        score_moves(pos, tt_move);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        std::transform(std::begin(moves), std::begin(moves) + count, std::begin(scores),
                       [&pos, tt_move](Move move)
                       {
                           return score_move(pos, move, tt_move);
                       });
    }

    Move MovePicker::next_move()
    {
        if (current_index >= count)
        {
            return Move{};
        }

        const auto best_score = std::max_element(std::begin(scores) + current_index, std::begin(scores) + count);
        const int best_index = static_cast<int>(std::distance(std::begin(scores), best_score));

        std::swap(moves[current_index], moves[best_index]);
        std::swap(scores[current_index], scores[best_index]);
        return moves[current_index++];
    }
} // namespace MoveOrdering