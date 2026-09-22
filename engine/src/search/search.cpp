#include "search.h"
#include "movegen.h"
#include "position.h"
#include "types.h"
#include <iostream>

namespace Search
{
    /** @brief Global atomic flag used to interrupt search thread asynchronously (e.g., UCI 'stop' or time out). */
    std::atomic<bool> Signals_stop{false};

    /**
     * @brief Resets engine search flags and signal controls.
     */
    void init()
    {
        Signals_stop.store(false, std::memory_order_relaxed);
    }

    Value quiescence(Position& pos, int ply, Value alpha, Value beta, SearchContext& ctx)
    {
        // Track leaf evaluation node count for UCI statistics
        ctx.info.nodes++;

        // Stand-pat evaluation: Assume the side to move can choose not to capture
        // Stub implementation defaults to VALUE_ZERO; will integrate Eval::evaluate(pos)
        Value stand_pat = VALUE_ZERO;

        // Beta cutoff: Position is already better than the opponent will allow
        if (stand_pat >= beta)
            return beta;

        // Raise lower bound if stand-pat score exceeds current alpha
        if (stand_pat > alpha)
            alpha = stand_pat;

        return alpha;
    }

    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null)
    {
        // Compile-time constant flags to optimize branch elimination
        constexpr bool IsPV = (NT != NON_PV);
        constexpr bool IsRoot = (NT == ROOT);

        // Check for asynchronous interrupt signals (UCI 'stop' command or time limit breach)
        if (Signals_stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Leaf Node Hand-off: Transition to Quiescence Search when nominal depth expires
        if (depth <= 0)
        {
            return quiescence(pos, ply, alpha, beta, ctx);
        }

        ctx.info.nodes++;

        // Check detection for tactical evaluation and pruning restrictions
        bool in_check = pos.in_check();

        // Compile-time check: Stripped out completely for PV and ROOT nodes.
        // If giving up a turn still results in a score >= beta, prune the subtree.
        if constexpr (!IsPV)
        {
            if (do_null && depth >= 3 && !in_check)
            {
                pos.make_move(Move::null());
                // Search with reduced depth (R = 2) and a zero-window [-beta, -beta + 1]
                Value null_score = -negamax<NON_PV>(pos, depth - 1 - 2, ply + 1, -beta, -beta + 1, ctx, false);
                pos.unmake_move(Move::null());

                // Beta cutoff from null move (fail-high)
                if (null_score >= beta)
                {
                    return beta;
                }
            }
        }

        // Generate legal moves for the current position
        MoveList moves;
        MoveGen::generate_legal_moves(pos, moves);

        // Terminal Node Handling: Checkmate or Stalemate
        if (moves.count == 0)
        {
            // Adjust mate score by ply to prefer shorter mate paths
            return in_check ? (-VALUE_MATE + ply) : VALUE_ZERO;
        }

        Value best_score = -VALUE_INFINITE;
        Move best_move = Move::none();
        int moves_searched = 0;

        // Move iteration loop
        for (int i = 0; i < moves.count; ++i)
        {
            const Move move = moves.moves[i];
            pos.make_move(move);
            Value score = -VALUE_INFINITE;

            if constexpr (IsPV)
            {
                if (moves_searched == 0)
                {
                    // Full PV window search for the primary candidate move
                    score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                }
                else
                {
                    // Zero-window search (Null Window Search) for late moves to prove inferiority
                    score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -alpha - 1, -alpha, ctx, true);

                    // Re-search with full window if zero-window search fails high
                    if (score > alpha && score < beta)
                    {
                        score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                    }
                }
            }
            else
            {
                // Non-PV nodes always propagate NON_PV template with standard bounds
                score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
            }

            pos.unmake_move(move);
            moves_searched++;

            // Abort current move evaluation if stop signal was raised
            if (Signals_stop.load(std::memory_order_relaxed))
                return VALUE_ZERO;

            // Alpha-Beta Score Update Logic
            if (score > best_score)
            {
                best_score = score;
                best_move = move;

                if (score > alpha)
                {
                    alpha = score;

                    // Update best move at root level for UCI output
                    if constexpr (IsRoot)
                    {
                        ctx.info.best_move = move;
                    }

                    // Beta Cutoff (Fail-High)
                    if (alpha >= beta)
                    {
                        // Store Killer Move heuristic for non-captures to improve move ordering in future searches
                        if (!move.is_capture() && ply < MAX_PLY)
                        {
                            ctx.killer_moves[ply][1] = ctx.killer_moves[ply][0];
                            ctx.killer_moves[ply][0] = move;
                        }
                        break; // Cut off remaining move searches
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

        // Iterative Deepening Loop
        for (int current_depth = 1; current_depth <= limits.depth; ++current_depth)
        {
            if (Signals_stop.load(std::memory_order_relaxed))
                break;

            ctx.info.depth = current_depth;

            // Invoke root search template
            Value score = negamax<ROOT>(pos, current_depth, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

            // Output UCI telemetry progress update
            if (!Signals_stop.load(std::memory_order_relaxed))
            {
                std::cout << "info depth " << current_depth << " score cp " << score << " nodes " << ctx.info.nodes << std::endl;
            }
        }

        // Output final UCI choice
        std::cout << "bestmove " << ctx.info.best_move << std::endl;
    }

    // Explicit template instantiations to optimize compilation boundaries
    template Value negamax<ROOT>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<NON_PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);

} // namespace Search
