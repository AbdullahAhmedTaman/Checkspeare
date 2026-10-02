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

        TTEntry tt_entry;
        Move tt_move = Move::none();
        Value alpha_orig = alpha;

        bool tt_hit = TT.probe(pos.key, ply, tt_entry);
        if (tt_hit)
        {
            tt_move = tt_entry.best_move;
            if (tt_entry.flag() == TT_EXACT)
                return tt_entry.score;
            if (tt_entry.flag() == TT_LOWERBOUND && tt_entry.score >= beta)
                return tt_entry.score;
            if (tt_entry.flag() == TT_UPPERBOUND && tt_entry.score <= alpha)
                return tt_entry.score;
        }

        Value stand_pat = 0;

        if (!in_check)
        {
            stand_pat = Eval::evaluate(pos);
            if (stand_pat >= beta)
                return beta;
            if (stand_pat > alpha)
                alpha = stand_pat;

            constexpr Value DELTA_MARGIN = 1100;
            if (stand_pat + DELTA_MARGIN <= alpha)
            {
                return alpha;
            }
        }

        Value best_score = in_check ? -VALUE_INFINITE : stand_pat;
        Move best_move = Move::none();
        // Instantiate Lazy MovePicker (qsearch = true)
        MoveOrdering::MovePicker picker(pos, tt_move, ply, true);
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
            {
                best_score = score;
                best_move = move;
            }

            if (best_score >= beta)
                break;

            if (best_score > alpha)
                alpha = best_score;
        }

        if (in_check && legal_moves == 0)
            return -VALUE_MATE + ply;

        if (!ctx.stop_flag.load(std::memory_order_relaxed))
        {
            TTFlag flag = (best_score >= beta) ? TT_LOWERBOUND : (best_score > alpha_orig) ? TT_EXACT : TT_UPPERBOUND;
            TT.store(pos.key, ply, 0, best_score, flag, best_move);
        }
        return best_score;
    }
} // namespace Search