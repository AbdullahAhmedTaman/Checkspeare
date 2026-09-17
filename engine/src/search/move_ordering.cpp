#include "move_ordering.h"
#include <types.h>

namespace MoveOrdering
{
    namespace
    {
        void copy_moves(const MoveList& list, Move* moves, int index, int count)
        {
            if (index == count)
                return;

            moves[index] = list.moves[index];
            copy_moves(list, moves, index + 1, count);
        }

        void swap_moves(Move& first_move, int& first_score, Move& second_move, int& second_score)
        {
            Move move = first_move;
            first_move = second_move;
            second_move = move;

            int score = first_score;
            first_score = second_score;
            second_score = score;
        }

        void score_moves_recursive(const Position& pos, Move* moves, int* scores, int index, int count, Move tt_move)
        {
            if (index == count)
                return;

            Move move = moves[index];
            scores[index] = 0;
            if (move == tt_move)
            {
                scores[index] = TT_MOVE_SCORE;
            }
            else if (move.flag() == MoveFlag::CAPTURE || move.flag() == MoveFlag::EP_CAPTURE)
            {
                Piece attacker = pos.board[move.from()];
                Piece victim = pos.board[move.to()];
                PieceType attacker_type = type_of(attacker);
                PieceType victim_type = type_of(victim);

                if (move.flag() == MoveFlag::EP_CAPTURE)
                    victim_type = PieceType::PAWN;

                if (attacker_type != PieceType::NONE && victim_type != PieceType::NONE)
                    scores[index] = CAPTURE_BONUS + MVV_LVA[victim_type][attacker_type];
            }

            score_moves_recursive(pos, moves, scores, index + 1, count, tt_move);
        }

        int find_best_index(const int* scores, int index, int count, int best_index, int best_score)
        {
            if (index == count)
                return best_index;

            if (scores[index] > best_score)
                return find_best_index(scores, index + 1, count, index, scores[index]);

            return find_best_index(scores, index + 1, count, best_index, best_score);
        }
    } // namespace

    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move, int)
        : count(list.size()),
          current_index(0)
    {
        copy_moves(list, moves, 0, count);
        score_moves(pos, tt_move);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        score_moves_recursive(pos, moves, scores, 0, count, tt_move);
    }

    Move MovePicker::next_move()
    {
        if (current_index >= count)
        {
            return Move{};
        }

        int best_index = find_best_index(scores, current_index, count, current_index, -1);

        swap_moves(moves[current_index], scores[current_index], moves[best_index], scores[best_index]);
        return moves[current_index++];
    }
} // namespace MoveOrdering