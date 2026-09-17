#include "move_ordering.h"
#include "movegen.h"
#include "search.h"


extern Value evaluate(const Position& pos);

namespace Search
{
    Value quiescence(Position& pos, int ply, Value alpha, Value beta)
    {
        Value stand_pat = evaluate(pos);

        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        MoveList list;
        MoveGen::generate_pseudo_legal_moves(pos, list);

        MoveOrdering::MovePicker picker(list, pos, Move::none());

        // Iteration Step: move = picker.next_move()
        for (Move move = picker.next_move(); move.is_ok(); move = picker.next_move())
        {
            if (!move.is_capture())
            {
                continue; // Skips to the next iteration step automatically
            }

            pos.make_move(move);

            Color us = (pos.sideToMove == WHITE) ? BLACK : WHITE;
            if (pos.is_square_attacked(pos.king_square(us), pos.sideToMove))
            {
                pos.unmake_move(move);
                continue;
            }

            Value score = -quiescence(pos, ply + 1, -beta, -alpha);

            pos.unmake_move(move);

            if (score >= beta)
                return beta;
            if (score > alpha)
                alpha = score;
        }

        return alpha;
    }
} // namespace Search