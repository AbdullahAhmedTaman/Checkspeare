#include "move_ordering.h"
#include "heuristics.h"
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

        int promotion_score(Move move)
        {
            if (!move.is_promotion())
                return 0;

            return PROMOTION_BONUS + static_cast<int>(promotion_type(move)) * 10;
        }

        int capture_score(const Position& pos, Move move)
        {
            if (!move.is_capture())
                return 0;

            const Piece attacker = pos.board[move.from()];
            const PieceType attacker_type = type_of(attacker);
            PieceType victim_type = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);

            if (attacker_type == NONE || victim_type == NONE)
                return 0;

            return CAPTURE_BONUS + MVV_LVA[victim_type][attacker_type] + promotion_score(move);
        }

        int quiet_score(const Position& pos, Move move, int ply)
        {
            if (move.is_capture() || move.is_promotion())
                return promotion_score(move);

            const Piece piece = pos.board[move.from()];
            const bool is_killer = ply >= 0 && ply < Search::MAX_PLY &&
                                   (Search::killer_moves[ply][0] == move || Search::killer_moves[ply][1] == move);
            return (is_killer ? KILLER_SCORE : 0) + Search::history_score(move, piece);
        }

        void score_moves_recursive(const Position& pos, Move* moves, int* scores, int index, int count, Move tt_move, int ply)
        {
            if (index == count)
                return;

            Move move = moves[index];
            scores[index] = 0;
            if (move == tt_move)
            {
                scores[index] = TT_MOVE_SCORE;
            }
            else if (move.is_capture())
                scores[index] = capture_score(pos, move);
            else
                scores[index] = quiet_score(pos, move, ply);

            score_moves_recursive(pos, moves, scores, index + 1, count, tt_move, ply);
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

    MovePicker::MovePicker(const MoveList& list, const Position& pos, Move tt_move, int ply)
        : count(list.size()),
          current_index(0),
          ply(ply)
    {
        copy_moves(list, moves, 0, count);
        score_moves(pos, tt_move);
    }

    void MovePicker::score_moves(const Position& pos, Move tt_move)
    {
        score_moves_recursive(pos, moves, scores, 0, count, tt_move, ply);
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