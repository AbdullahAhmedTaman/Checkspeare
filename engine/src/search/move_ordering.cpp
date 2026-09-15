#include "move_ordering.h"
#include <algorithm>

namespace MoveOrdering
{
    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move)
        : count(list.size()),
          current_index(0)
    {
        for (int i = 0; i < count; ++i)
        {
            moves[i].move = list.begin()[i];
            moves[i].score = 0;
        }
        score_moves(pos, tt_move);
    }

    PieceType MovePicker::get_piece_type(Piece p) const
    {
        if (p == PIECE_NONE)
            return PieceType::NONE;
        return static_cast<PieceType>(p % 6);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        for (int i = 0; i < count; ++i)
        {
            Move m = moves[i].move;
            if (m == tt_move)
            {
                moves[i].score = TT_MOVE_SCORE;
                continue;
            }
            if (m.flag() == MoveFlag::CAPTURE || m.flag() == MoveFlag::EP_CAPTURE)
            {
                Piece attacker = pos.board[m.from()];
                Piece victim = pos.board[m.to()];

                PieceType attacker_type = get_piece_type(attacker);
                PieceType victim_type = get_piece_type(victim);

                if (m.flag() == MoveFlag::EP_CAPTURE)
                    victim_type = PieceType::PAWN;

                if (attacker_type != PieceType::NONE && victim_type != PieceType::NONE)
                {
                    moves[i].score = CAPTURE_BONUS + MVV_LVA[victim_type][attacker_type];
                }
            }
            else
            {
                moves[i].score = 0;
            }
        }
    }

    Move MovePicker::next_move()
    {
        if (current_index >= count)
        {
            return Move{};
        }

        int best_score = -1;
        int best_index = current_index;

        for (int i = current_index; i < count; ++i)
        {
            if (moves[i].score > best_score)
            {
                best_score = moves[i].score;
                best_index = i;
            }
        }

        std::swap(moves[current_index], moves[best_index]);
        return moves[current_index++].move;
    }
} // namespace MoveOrdering