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

            if (move.is_promotion())
            {
                PieceType pt = promotion_type(move);
                if (pt == QUEEN)
                {
                    if (move.is_capture())
                    {
                        const PieceType victim = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);
                        return PROMOTION_QUEEN + MVV_LVA[victim][PAWN];
                    }
                    return PROMOTION_QUEEN;
                }
                else if (pt == KNIGHT)
                {
                    if (move.is_capture())
                    {
                        const PieceType victim = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);
                        return PROMOTION_KNIGHT + MVV_LVA[victim][PAWN];
                    }
                    return PROMOTION_KNIGHT;
                }

                // Minor promotions drop straight to the bottom
                return PROMOTION_MINOR;
            }
            const Piece attacker = pos.board[move.from()];

            if (move.is_capture())
            {
                const PieceType attacker_type = type_of(attacker);
                const PieceType victim_type = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);

                if (attacker_type != NONE && victim_type != NONE)
                {
                    if (attacker_type <= victim_type)
                    {
                        return GOOD_CAPTURE + MVV_LVA[victim_type][attacker_type];
                    }
                    else
                    {
                        return BAD_CAPTURE + MVV_LVA[victim_type][attacker_type];
                    }
                }
                return GOOD_CAPTURE;
            }

            if (ply >= 0 && ply < MAX_PLY)
            {
                if (Search::killer_moves[ply][0] == move)
                    return KILLER_SCORE_1;
                if (Search::killer_moves[ply][1] == move)
                    return KILLER_SCORE_2;
            }

            // 5. Quiet Moves (History Heuristic)
            return Search::history_score(move, attacker);
        }
    } // namespace

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
    }

    Move MovePicker::next_move()
    {
        if (count == 0)
        {
            return Move{};
        }

        int best_idx = 0;
        for (int i = 1; i < count; ++i)
        {
            if (moves[i].score > moves[best_idx].score)
            {
                best_idx = i;
            }
        }

        Move best_move = moves[best_idx].move;

        moves[best_idx] = moves[count - 1];
        --count;

        return best_move;
    }
} // namespace MoveOrdering