#include "search.h"
#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"
#include "eval/evaluate.h"
#include "heuristics.h"
#include "move_ordering.h"
#include "transposition.h"
#include "uci/uci.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>

bool is_repetition(const Position& pos);

namespace Search
{
    Move killer_moves[MAX_PLY][2];
    int history_table[MAX_PIECES][MAX_SQUARES];

    bool try_null_move_pruning(Position& pos, int depth, int ply, Value beta, SearchContext& ctx, Value& out_score)
    {
        Value static_eval = Eval::evaluate(pos);
        Color us = pos.sideToMove;

        // Zugzwang Material Guard
        Bitboard non_pawn_material =
            pos.pieces[us][KNIGHT] | pos.pieces[us][BISHOP] | pos.pieces[us][ROOK] | pos.pieces[us][QUEEN];

        // Static Evaluation Guard
        if (static_eval >= beta && non_pawn_material != 0)
        {
            // Adaptive Reduction
            int R = 2 + (depth / 4);

            pos.make_move(Move::null());

            // Zero-window search at reduced depth. do_null = false prevents double-nulling.
            Value null_score = -negamax<NON_PV>(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, ctx, false);

            pos.unmake_move(Move::null());

            // Beta Cutoff Evaluation
            if (null_score >= beta)
            {
                // Safeguard against artificial mate scores
                out_score = (null_score >= MATE_BOUND) ? beta : null_score;
                return true;
            }
        }

        return false;
    }

    int LMRTable[MAX_PLY][MAX_MOVES];

    /**
     * @brief Precomputes the baseline Late Move Reduction values using a logarithmic curve.
     *        Called once during engine startup.
     */
    void init_lmr_table()
    {
        for (int depth = 0; depth < MAX_PLY; ++depth)
        {
            for (int moves = 0; moves < MAX_MOVES; ++moves)
            {
                // Standard LMR logarithmic scaling formula
                if (depth > 0 && moves > 0)
                {
                    LMRTable[depth][moves] = static_cast<int>(0.75 + std::log(depth) * std::log(moves) / 2.25);
                }
                else
                {
                    LMRTable[depth][moves] = 0;
                }
            }
        }
    }

    std::atomic<bool> Signals_stop{false};

    void init()
    {
        Signals_stop.store(false, std::memory_order_relaxed);
    }

    /**
     * @brief Calculates the depth reduction for Late Move Reductions (LMR).
     *        Returns 0 if LMR should be skipped (hard filters).
     */
    inline int calculate_lmr_reduction(bool is_pv, int depth, int moves_searched, bool in_check, const Move& move,
                                       const Move* killers)
    {
        // Do not reduce if depth is low, it's an early move, in check, or tactical
        if (depth < 3 || moves_searched < 3 || in_check || move.is_capture() || move.is_promotion())
        {
            return 0; // 0 means do not reduce
        }

        int safe_depth = std::min(depth, MAX_PLY - 1);
        int safe_moves = std::min(moves_searched, MAX_MOVES - 1);
        int R = LMRTable[safe_depth][safe_moves];
        if (is_pv)
        {
            R -= 1; // Search PV nodes slightly deeper
        }

        if (move == killers[0] || move == killers[1])
        {
            R -= 1;
        }

        // Clamp the reduction to remain within safe bounds
        return std::clamp(R, 0, depth - 1);
    }

    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null)
    {
        if (ply >= MAX_PLY - 1)
            return Eval::evaluate(pos);

        // Mate Distance Pruning: clamp search bounds relative to distance from root
        alpha = std::max(alpha, -VALUE_MATE + ply);
        beta = std::min(beta, VALUE_MATE - ply - 1);
        Value alpha_orig = alpha;

        if (alpha >= beta)
            return alpha;

        // Compile-time constant flags to optimize branch elimination
        constexpr bool IsPV = (NT != NON_PV);
        constexpr bool IsRoot = (NT == ROOT);

        // Check for asynchronous interrupt signals (UCI 'stop' command or time limit breach)
        if (Signals_stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Draw Detection: 50-move rule and Zobrist repetition
        if (ply > 0 && (pos.halfmoveClock >= 100 || is_repetition(pos)))
        {
            return VALUE_ZERO;
        }

        // Check detection for tactical evaluation and pruning restrictions
        bool in_check = pos.in_check();

        // Check Extension: Force evasion resolution in main loop before dropping into Q-search
        if (in_check && ply < MAX_PLY - 1)
        {
            depth++;
        }

        // Leaf Node Hand-off: Transition to Quiescence Search when nominal depth expires
        if (depth <= 0)
        {
            return quiescence(pos, ply, alpha, beta, ctx);
        }

        ctx.info.nodes++;
        // Poll the wall clock every 1024 nodes. Hard ceiling aborts immediately;
        // soft target also aborts, but only below the root so that we always
        // produce a best_move for the current depth before giving up.
        if ((ctx.info.nodes & 1023) == 0 && ctx.opt_time_ms > 0)
        {
            auto now = std::chrono::steady_clock::now();
            uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start_time).count();

            if (elapsed >= ctx.max_time_ms)
            {
                Signals_stop.store(true, std::memory_order_relaxed);
            }
            else if (elapsed >= ctx.opt_time_ms && ply > 0)
            {
                Signals_stop.store(true, std::memory_order_relaxed);
            }
        }

        // Compile-time check: Stripped out completely for PV and ROOT nodes.
        if constexpr (!IsPV)
        {
            // Only try NMP if permitted, depth is sufficient, and king is safe.
            if (do_null && depth >= 3 && !in_check)
            {
                Value nmp_score;
                if (try_null_move_pruning(pos, depth, ply, beta, ctx, nmp_score))
                {
                    return nmp_score; // Cutoff achieved, prune the subtree
                }
            }
        }

        TTEntry tt_entry;
        Move tt_move = Move::none();
        bool tt_hit = TT.probe(pos.key, ply, tt_entry);

        if (tt_hit)
        {
            tt_move = tt_entry.best_move;

            if (tt_entry.depth >= depth && !IsRoot)
            {
                if (tt_entry.flag == TT_EXACT)
                    return tt_entry.score;
                if (tt_entry.flag == TT_LOWERBOUND && tt_entry.score >= beta)
                    return tt_entry.score;
                if (tt_entry.flag == TT_UPPERBOUND && tt_entry.score <= alpha)
                    return tt_entry.score;
            }
        }

        // Generate pseudo-legal moves for performance (lazy king-safety filtering)
        MoveList moves;
        MoveGen::generate_pseudo_legal_moves(pos, moves);

        static const Move empty_killers[2] = {Move::none(), Move::none()};
        const Move* current_killers = (ply < MAX_PLY) ? Search::killer_moves[ply] : empty_killers;
        // Instantiate MovePicker with TT best move hint for move ordering
        MoveOrdering::MovePicker picker(moves, pos, tt_move, ply);

        Value best_score = -VALUE_INFINITE;
        Move best_move = Move::none();
        int moves_searched = 0;
        int legal_moves_count = 0;

        const Color us = pos.sideToMove;

        // Move iteration loop driven by MovePicker
        while (true)
        {
            const Move move = picker.next_move();
            if (!move.is_ok())
                break;

            // Lazy Legality Check: execute move and verify our King is not under attack
            pos.make_move(move);
            if (pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1)))
            {
                pos.unmake_move(move);
                continue; // Skip illegal move
            }

            legal_moves_count++;
            Value score = -VALUE_INFINITE;

            bool needs_full_search = true;

            int R = calculate_lmr_reduction(IsPV, depth, moves_searched, in_check, move, current_killers);
            // If R > 0, the helper function approved the reduction
            if (R > 0)
            {
                // Execute Shallow Zero-Window Search
                Value reduced_score = -negamax<NON_PV>(pos, depth - 1 - R, ply + 1, -alpha - 1, -alpha, ctx, true);

                // If it fails low, the move is truly bad. Skip the full search.
                if (reduced_score <= alpha)
                {
                    score = reduced_score;
                    needs_full_search = false;
                }
            }

            if (needs_full_search)
            {
                if constexpr (IsPV)
                {
                    if (moves_searched == 0)
                    {
                        score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                    }
                    else
                    {
                        score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -alpha - 1, -alpha, ctx, true);
                        if (score > alpha && score < beta)
                        {
                            score = -negamax<PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                        }
                    }
                }
                else
                {
                    score = -negamax<NON_PV>(pos, depth - 1, ply + 1, -beta, -alpha, ctx, true);
                }
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
                        // Store Killer Move heuristic for non-captures
                        if (!move.is_capture() && !move.is_promotion())
                        {
                            if (ply < MAX_PLY)
                                Search::update_killers(move, ply);
                            Search::update_history(move, depth, pos.board[move.from()]);
                        }
                        break; // Cut off remaining move searches
                    }
                }
            }
        }

        // Terminal Node Handling: Checkmate or Stalemate
        if (legal_moves_count == 0)
        {
            // Adjust mate score by ply to prefer shorter mate paths
            return in_check ? (-VALUE_MATE + ply) : VALUE_ZERO;
        }

        if (!Signals_stop.load(std::memory_order_relaxed))
        {
            TTFlag flag = (best_score >= beta) ? TT_LOWERBOUND : (best_score > alpha_orig) ? TT_EXACT : TT_UPPERBOUND;

            TT.store(pos.key, ply, depth, best_score, flag, best_move);
        }

        return best_score;
    }

    void start_search(Position& pos, const SearchLimits& limits)
    {
        init();
        SearchContext ctx(limits, Signals_stop);

        uint64_t time_left = (pos.sideToMove == WHITE) ? limits.wtime : limits.btime;
        uint64_t inc = (pos.sideToMove == WHITE) ? limits.winc : limits.binc;

        if (limits.movetime > 0)
        {
            ctx.opt_time_ms = (limits.movetime > 20) ? limits.movetime - 20 : 1;
            ctx.max_time_ms = ctx.opt_time_ms;
        }
        else if (time_left > 0)
        {
            ctx.opt_time_ms = (time_left / 40) + (inc * 3 / 4);
            // Cap the hard ceiling at ~5% of remaining time. A larger margin
            // (100 ms) is reserved for GUI round-trip latency and clock drift.
            uint64_t safe_margin = (time_left > 100) ? time_left - 100 : time_left / 2;
            ctx.max_time_ms = std::min(time_left / 20, safe_margin);

            if (ctx.max_time_ms == 0)
                ctx.max_time_ms = 1;

            ctx.opt_time_ms = std::min(ctx.opt_time_ms, ctx.max_time_ms);
        }
        else
        {
            ctx.opt_time_ms = UINT64_MAX;
            ctx.max_time_ms = UINT64_MAX;
        }

        // Retain move from the last fully completed depth iteration
        Move safe_best_move = Move::none();

        MoveList root_moves;
        MoveGen::generate_legal_moves(pos, root_moves);
        if (root_moves.count > 0)
            safe_best_move = root_moves.moves[0];

        const uint64_t base_opt_time_ms = ctx.opt_time_ms;
        double time_extension_factor = 1.0;

        // Iterative Deepening Loop
        Value prev_score = VALUE_ZERO;
        for (int current_depth = 1; current_depth <= limits.depth; ++current_depth)
        {
            if (Signals_stop.load(std::memory_order_relaxed))
                break;

            ctx.info.depth = current_depth;
            Value score = VALUE_ZERO;

            // search with a full window at low depths, as evaluation is still unstable
            if (current_depth <= 4)
            {
                score = negamax<ROOT>(pos, current_depth, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
            }
            else
            {
                int delta = 25;
                Value alpha = std::max(prev_score - delta, -VALUE_INFINITE);
                Value beta = std::min(prev_score + delta, VALUE_INFINITE);

                while (!Signals_stop.load(std::memory_order_relaxed))
                {
                    score = negamax<ROOT>(pos, current_depth, 0, alpha, beta, ctx, false);

                    if (Signals_stop.load(std::memory_order_relaxed))
                        break;

                    if (score <= alpha)
                    {
                        alpha = std::max(alpha - delta, -VALUE_INFINITE);
                        delta += delta / 2;
                    }

                    else if (score >= beta)
                    {
                        beta = std::min(beta + delta, VALUE_INFINITE);
                        delta += delta / 2;
                    }

                    else
                    {
                        break;
                    }

                    // If delta grows too wide, fall back to a full window
                    if (delta >= 1000)
                    {
                        alpha = -VALUE_INFINITE;
                        beta = VALUE_INFINITE;
                    }
                }
            }

            // Discard partial depth results if interrupted mid-search
            if (Signals_stop.load(std::memory_order_relaxed))
                break;

            auto now = std::chrono::steady_clock::now();
            uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start_time).count();

            // Only extend time if we aren't on an infinite search
            if (current_depth > 1 && base_opt_time_ms != UINT64_MAX)
            {
                bool move_changed = (ctx.info.best_move != safe_best_move);
                bool score_volatile = (std::abs(score - prev_score) >= 50);

                if (move_changed || score_volatile)
                {
                    // Add to the multiplier, but cap it so we don't consume the entire clock
                    time_extension_factor = std::min(time_extension_factor + 0.5, 2.5);

                    // Multiply against the unmutated base time, not the already-extended time
                    ctx.opt_time_ms = std::min(static_cast<uint64_t>(base_opt_time_ms * time_extension_factor), ctx.max_time_ms);
                }
            }

            safe_best_move = ctx.info.best_move;
            prev_score = score;
            ctx.info.score = score;
            ctx.info.time_ms = elapsed;

            // Output UCI telemetry progress update
            UCI::send_search_info(ctx.info);

            if (elapsed >= ctx.opt_time_ms)
                break;

            // Hard ceiling check between depths: protects against a single depth
            // iteration running away past max_time_ms without the in-search poll
            // catching it in time (e.g., very slow nodes on old hardware).
            if (elapsed >= ctx.max_time_ms)
                break;

            // Checkmate early exit
            if (std::abs(score) >= MATE_BOUND && current_depth >= (VALUE_MATE - std::abs(score)))
                break;
        }

        // Output final UCI choice
        std::cout << "bestmove " << safe_best_move << std::endl;
    }

    // Explicit template instantiations to optimize compilation boundaries
    template Value negamax<ROOT>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<NON_PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);

} // namespace Search