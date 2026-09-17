#include "evaluate.h"
#include "move_ordering.h"
#include "movegen.h"
#include "search.h"

namespace Search
{
    namespace
    {
        Value search_captures(Position& pos, MoveOrdering::MovePicker& picker, int ply, Value alpha, Value beta)
        {
            Move move = picker.next_move();
            if (!move.is_ok())
                return alpha;

            if (!move.is_capture())
                return search_captures(pos, picker, ply, alpha, beta);

            pos.make_move(move);

            Color us = static_cast<Color>(pos.sideToMove ^ 1);
            bool legal = !pos.is_square_attacked(pos.king_square(us), pos.sideToMove);
            if (!legal)
            {
                pos.unmake_move(move);
                return search_captures(pos, picker, ply, alpha, beta);
            }

            Value score = -quiescence(pos, ply + 1, -beta, -alpha);
            pos.unmake_move(move);

            if (score >= beta)
                return beta;
            if (score > alpha)
                alpha = score;

            return search_captures(pos, picker, ply, alpha, beta);
        }
    } // namespace

    Value quiescence(Position& pos, int ply, Value alpha, Value beta)
    {
        Value stand_pat = Eval::evaluate(pos);

        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        MoveList list;
        MoveGen::generate_pseudo_legal_moves(pos, list);

        MoveOrdering::MovePicker picker(list, pos, Move::none(), ply);
        return search_captures(pos, picker, ply, alpha, beta);
    }
} // namespace Search