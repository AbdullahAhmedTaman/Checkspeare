#include "search.h"
#include "movegen.h"
#include "position.h"
#include "types.h"
#include <iostream>

namespace Search
{
    std::atomic<bool> Signals_stop{false};

    void init()
    {
        Signals_stop.store(false, std::memory_order_relaxed);
    }

    Value quiescence(Position& pos, int ply, Value alpha, Value beta, SearchContext& ctx)
    {
        ctx.info.nodes++;

        Value stand_pat = VALUE_ZERO;

        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        return alpha;
    }

    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null)
    {
        constexpr bool IsPV = (NT != NON_PV);
        constexpr bool IsRoot = (NT == ROOT);

        // Check for interrupt signals / time limit breach
        if (Signals_stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Leaf Node Hand-off
        if (depth <= 0)
        {
            return quiescence(pos, ply, alpha, beta, ctx);
        }

        ctx.info.nodes++;

        bool in_check = pos.in_check();

        // Null-Move Pruning Hook (Stripped out entirely at compile-time for PV/ROOT)
        if constexpr (!IsPV)
        {
            if (do_null && depth >= 3 && !in_check)
            {
                pos.make_move(Move::null());
                Value null_score = -negamax<NON_PV>(pos, depth - 1 - 2, ply + 1, -beta, -beta + 1, ctx, false);
                pos.unmake_move(Move::null());

                if (null_score >= beta)
                {
                    return beta;
                }
            }
        }

        // Generate legal moves using MoveGen namespace API
        MoveList moves;
        MoveGen::generate_legal_moves(pos, moves);

        if (moves.count == 0)
        {
            return in_check ? (-VALUE_MATE + ply) : VALUE_ZERO;
        }

        Value best_score = -VALUE_INFINITE;
        Move best_move = Move::none();
        int moves_searched = 0;

        for (int i = 0; i < moves.count; ++i)
        {
            const Move move = moves.moves[i];
            pos.make_move(move);
            Value score = -VALUE_INFINITE;

            // Template-based recursive dispatch
            if constexpr (IsPV)
            {
                if (moves_searched == 0)
                {
                    // Full PV window for first move in PV nodes
                    score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                }
                else
                {
                    // Zero-window search under NON_PV template for late moves
                    score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -alpha - 1, -alpha, ctx, true);
                    if (score > alpha && score < beta)
                    {
                        // Re-search with full window if zero-window search fails high
                        score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                    }
                }
            }
            else
            {
                // Non-PV nodes always propagate NON_PV template
                score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
            }

            pos.unmake_move(move);
            moves_searched++;

            if (Signals_stop.load(std::memory_order_relaxed))
                return VALUE_ZERO;

            if (score > best_score)
            {
                best_score = score;
                best_move = move;

                if (score > alpha)
                {
                    alpha = score;

                    if constexpr (IsRoot)
                    {
                        ctx.info.best_move = move;
                    }

                    if (alpha >= beta)
                    {
                        // Store Killer Move (Engineer 2 Hook)
                        if (!move.is_capture() && ply < MAX_PLY)
                        {
                            ctx.killer_moves[ply][1] = ctx.killer_moves[ply][0];
                            ctx.killer_moves[ply][0] = move;
                        }
                        break; // Beta Cutoff
                    }
                }
            }
        }

        return best_score;
    }

    void start_search(Position& pos, const SearchLimits& limits)
    {
        init();
        SearchContext ctx(limits, Signals_stop);

        for (int current_depth = 1; current_depth <= limits.depth; ++current_depth)
        {
            if (Signals_stop.load(std::memory_order_relaxed))
                break;

            ctx.info.depth = current_depth;

            Value score = negamax<ROOT>(pos, current_depth, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

            if (!Signals_stop.load(std::memory_order_relaxed))
            {
                std::cout << "info depth " << current_depth << " score cp " << score << " nodes " << ctx.info.nodes << std::endl;
            }
        }

        std::cout << "bestmove " << ctx.info.best_move << std::endl;
    }

    // Explicit template instantiations to optimize compilation boundaries
    template Value negamax<ROOT>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<NON_PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);

} // namespace Search