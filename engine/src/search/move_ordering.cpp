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

            if (move.is_promotion())
            {
                return promotion_score;
            }

            const bool is_killer =
                ply >= 0 && ply < MAX_PLY && (Search::killer_moves[ply][0] == move || Search::killer_moves[ply][1] == move);
            return (is_killer ? KILLER_SCORE : 0) + Search::history_score(move, attacker);
        }
    } // namespace

    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move, int ply)
        : count(list.size()),
          ply(ply)
    {
        for (int i = 0; i < count; ++i)
        {
            moves[i].move = list.moves[i];
        }
        score_moves(pos, tt_move);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        for (int i = 0; i < count; ++i)
        {
            moves[i].score = score_move(pos, moves[i].move, tt_move, ply);
        }
    }

    Move MovePicker::next_move()
    {
        if (count == 0)
        {
            return Move::none();
        }

        int best_idx = 0;
        int best_score = moves[0].score;

        for (int i = 1; i < count; ++i)
        {
            if (moves[i].score > best_score)
            {
                best_score = moves[i].score;
                best_idx = i;
            }
        }

        std::swap(moves[best_idx], moves[count - 1]);

        return moves[--count].move;
    }
} // namespace MoveOrdering