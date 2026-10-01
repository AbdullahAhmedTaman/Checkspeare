#include "board/movegen.h"
#include "eval/evaluate.h"
#include "move_ordering.h"
#include "search.h"
#include "transposition.h"


namespace Search
{
    namespace
    {

        /**
         * @brief Identifies moves that must be searched in quiescence.
         *
         * Check evasions are all tactical in this context; otherwise only
         * captures and promotions are searched to resolve forcing sequences.
         */
        bool is_tactical(Move move, bool in_check)
        {
            return in_check || move.is_capture() || move.is_promotion();
        }
    } // namespace

    /**
     * @brief Extends leaf search through forcing tactical moves.
     *
     * Stand-pat evaluation is used only when the side to move is not in
     * check. Legal captures, promotions, and check evasions are searched until
     * the position is quiet or the alpha-beta window is resolved.
     *
     * @param pos Current board position.
     * @param ply Current search ply.
     * @param alpha Lower alpha-beta bound.
     * @param beta Upper alpha-beta bound.
     * @param ctx Active search context and stop state.
     */
    Value quiescence(Position& pos, int ply, Value alpha, Value beta, SearchContext& ctx)
    {
        // Telemetry and Engine Hang Prevention
        ctx.info.nodes++;

        // Poll the clock every 2048 nodes to prevent deep tactical lines from bypassing time limits
        if ((ctx.info.nodes & 2047) == 0 && ctx.max_time_ms > 0)
        {
            auto now = std::chrono::steady_clock::now();
            uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start_time).count();
            if (elapsed >= ctx.max_time_ms)
            {
                ctx.stop_flag.store(true, std::memory_order_relaxed);
            }
        }

        if (ctx.stop_flag.load(std::memory_order_relaxed))
            return Eval::evaluate(pos);

        const Color us = pos.sideToMove;
        const bool in_check = pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1));

        if (ply >= MAX_SEARCH_PLY - 1)
            return Eval::evaluate(pos);

        Value stand_pat = 0;

        if (!in_check)
        {
            stand_pat = Eval::evaluate(pos);
            if (stand_pat >= beta)
                return beta;
            if (stand_pat > alpha)
                alpha = stand_pat;
        }

        Value best_score = in_check ? -VALUE_INFINITE : stand_pat;

        MoveList list;
        MoveGen::generate_pseudo_legal_moves(pos, list);

        if (!in_check)
        {
            int tactical_count = 0;
            for (int i = 0; i < list.count; ++i)
            {
                if (list.moves[i].is_capture() || list.moves[i].is_promotion())
                {
                    list.moves[tactical_count++] = list.moves[i];
                }
            }
            list.count = tactical_count;
        }

        MoveOrdering::MovePicker picker(list, pos, Move::none(), ply);
        int legal_moves = 0;

        while (true)
        {
            const Move move = picker.next_move();
            if (!move.is_ok())
                break;

            // Lazy Legality Check
            // If a beta cutoff occurs on move 1, the engine never pays the make_move / is_square_attacked
            // cost for the remaining 39 pseudo-legal moves.
            // This is a performance optimization that avoids unnecessary legality checks for moves that are unlikely to be
            // relevant.(generate_legal_moves)
            pos.make_move(move);
            if (pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1)))
            {
                pos.unmake_move(move);
                continue;
            }
            ++legal_moves;

            const Value score = -quiescence(pos, ply + 1, -beta, -alpha, ctx);
            pos.unmake_move(move);

            if (score > best_score)
                best_score = score;

            if (best_score >= beta)
                return beta;

            if (best_score > alpha)
                alpha = best_score;
        }

        if (in_check && legal_moves == 0)
            return -VALUE_MATE + ply;

        return best_score;
    }
} // namespace Search