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

    Move path_moves[MAX_PLY];
    Piece path_pieces[MAX_PLY];
    Move counter_moves[MAX_PIECES][MAX_SQUARES];
    int16_t cont_history[MAX_PIECES][MAX_SQUARES][PieceType::NONE][MAX_SQUARES];

    // >>> [EDIT START: Add Triangular PV Table for UCI PV line tracking]
    /** @brief Triangular Principal Variation table populated only at PV/ROOT nodes. */
    Move pv_table[MAX_PLY][MAX_PLY];
    int pv_length[MAX_PLY];
    // <<< [EDIT END]

    namespace
    {
        /**
         * @brief Internal NMP helper that accepts a precomputed static_eval from negamax
         *        so Eval::evaluate(pos) is never computed twice at the same node.
         */
        bool try_nmp_with_eval(Position& pos, int depth, int ply, Value beta, Value static_eval, SearchContext& ctx,
                               Value& out_score)
        {
            const Color us = pos.sideToMove;

            // Zugzwang Material Guard
            const Bitboard non_pawn_material =
                pos.pieces[us][KNIGHT] | pos.pieces[us][BISHOP] | pos.pieces[us][ROOK] | pos.pieces[us][QUEEN];

            // Static Evaluation Guard
            if (static_eval >= beta && non_pawn_material != 0)
            {
                // Adaptive Reduction
                const int R = 2 + (depth / 4);

                if (ply < MAX_PLY)
                {
                    path_moves[ply] = Move::null();
                    path_pieces[ply] = PIECE_NONE;
                }

                pos.make_move(Move::null());

                // Zero-window search at reduced depth. do_null = false prevents double-nulling.
                const Value null_score = -negamax<NON_PV>(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, ctx, false);

                pos.unmake_move(Move::null());

                if (Signals_stop.load(std::memory_order_relaxed))
                    return false;

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
    } // namespace

    bool try_null_move_pruning(Position& pos, int depth, int ply, Value beta, SearchContext& ctx, Value& out_score)
    {
        if (pos.in_check())
            return false;

        const Value static_eval = Eval::evaluate(pos);
        return try_nmp_with_eval(pos, depth, ply, beta, static_eval, ctx, out_score);
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
                if (depth > 0 && moves > 0)
                {
                    LMRTable[depth][moves] = static_cast<int>(0.75 + std::log(depth) * std::log(moves) / 1.80);
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
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(history_table, 0, sizeof(history_table));
        std::memset(path_moves, 0, sizeof(path_moves));
        std::memset(path_pieces, 0, sizeof(path_pieces));
        std::memset(counter_moves, 0, sizeof(counter_moves));
        std::memset(cont_history, 0, sizeof(cont_history));
        std::memset(pv_table, 0, sizeof(pv_table));
        std::memset(pv_length, 0, sizeof(pv_length));
    }

    /**
     * @brief Calculates the depth reduction for Late Move Reductions (LMR).
     *        Returns 0 if LMR should be skipped (hard filters).
     */
    inline int calculate_lmr_reduction(bool is_pv, int depth, int moves_searched, bool in_check, const Move& move,
                                       const Move* killers, Move counter, int hist_score)
    {
        // Reduce from the 2nd move (moves_searched >= 1) on non-PV nodes, and 4th move on PV nodes
        const int min_moves = is_pv ? 3 : 1;
        if (depth < 3 || moves_searched < min_moves || in_check || move.is_capture() || move.is_promotion())
        {
            return 0; // 0 means do not reduce
        }

        const int safe_depth = std::min(depth, MAX_PLY - 1);
        const int safe_moves = std::min(moves_searched, MAX_MOVES - 1);
        int R = LMRTable[safe_depth][safe_moves];

        if (!is_pv)
        {
            R += 1; // Search non-PV late quiet moves shallower
        }

        if (move == killers[0] || move == killers[1] || move == counter)
        {
            R -= 1; // Protect killer and countermove refutations
        }

        // Adjust reduction by move history score ([-2, +2] plies)
        R -= hist_score / 8192;

        // Clamp to [0, depth - 1] so depth 3 nodes can reduce by 2 plies into Q-search
        return std::clamp(R, 0, depth - 1);
    }

    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null)
    {
        // Compile-time constant flags to optimize branch elimination
        constexpr bool IsPV = (NT != NON_PV);
        constexpr bool IsRoot = (NT == ROOT);

        if constexpr (IsPV)
        {
            if (ply < MAX_PLY)
                pv_length[ply] = ply;
        }

        if (ply >= MAX_PLY - 1)
            return Eval::evaluate(pos);

        // Mate Distance Pruning: clamp search bounds relative to distance from root
        alpha = std::max(alpha, -VALUE_MATE + ply);
        beta = std::min(beta, VALUE_MATE - ply - 1);
        Value alpha_orig = alpha;
        if (alpha >= beta)
            return alpha;

        // Check for asynchronous interrupt signals (UCI 'stop' command or time limit breach)
        if (Signals_stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Draw Detection: 50-move rule and Zobrist repetition
        if (ply > 0 && (pos.halfmoveClock >= 100 || is_repetition(pos)))
        {
            return VALUE_ZERO;
        }

        TT.prefetch(pos.key);

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

        // Enforce max_nodes check during search traversal
        if (ctx.limits.max_nodes != UINT64_MAX && ctx.info.nodes >= ctx.limits.max_nodes)
        {
            Signals_stop.store(true, std::memory_order_relaxed);
            return VALUE_ZERO;
        }

        if ((ctx.info.nodes & 2047) == 0 && ctx.max_time_ms > 0)
        {
            auto now = std::chrono::steady_clock::now();
            uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start_time).count();
            if (elapsed >= ctx.max_time_ms)
            {
                Signals_stop.store(true, std::memory_order_relaxed);
                return VALUE_ZERO;
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
                if (tt_entry.flag() == TT_EXACT)
                {
                    if constexpr (IsPV)
                    {
                        if (ply < MAX_PLY && tt_move.is_ok())
                        {
                            pv_table[ply][ply] = tt_move;
                            pv_length[ply] = ply + 1;
                        }
                    }
                    return tt_entry.score;
                }
                if (tt_entry.flag() == TT_LOWERBOUND && tt_entry.score >= beta)
                    return tt_entry.score;
                if (tt_entry.flag() == TT_UPPERBOUND && tt_entry.score <= alpha)
                    return tt_entry.score;
            }
        }

        Value static_eval = -VALUE_INFINITE;
        const bool is_null_window = (beta - alpha <= 1);

        // Compile-time check: Stripped out completely for PV and ROOT nodes.
        if constexpr (!IsPV)
        {
            if (!in_check)
            {
                // Compute static evaluation exactly once per non-PV node
                static_eval = Eval::evaluate(pos);

                // 1. Reverse Futility Pruning (RFP / Static Null Move Pruning)
                if (is_null_window && depth <= 6 && std::abs(beta) < MATE_BOUND && static_eval - 75 * depth >= beta)
                {
                    return static_eval;
                }

                // 2. Null-Move Pruning (NMP) reusing the already-computed static_eval
                if (do_null && depth >= 3)
                {
                    Value nmp_score;
                    if (try_nmp_with_eval(pos, depth, ply, beta, static_eval, ctx, nmp_score))
                    {
                        return nmp_score; // Cutoff achieved, prune the subtree
                    }
                }
            }
        }

        static const Move empty_killers[2] = {Move::none(), Move::none()};
        const Move* current_killers = (ply < MAX_PLY) ? Search::killer_moves[ply] : empty_killers;

        Piece prev_piece = PIECE_NONE;
        Square prev_to = SQ_NONE;
        Move counter = Move::none();
        if (ply > 0 && ply < MAX_PLY)
        {
            const Move prev_move = path_moves[ply - 1];
            prev_piece = path_pieces[ply - 1];
            if (prev_move.is_ok() && !prev_move.is_null())
            {
                prev_to = prev_move.to();
                counter = counter_move(prev_piece, prev_to);
            }
        }

        const bool futility_prunable = (!IsPV && is_null_window && !in_check && depth <= 4 && std::abs(alpha) < MATE_BOUND &&
                                        static_eval + 120 + 100 * depth <= alpha);

        // Instantiate Lazy MovePicker with TT best move hint (qsearch = false)
        MoveOrdering::MovePicker picker(pos, tt_move, ply, false);
        Value best_score = -VALUE_INFINITE;
        Move best_move = Move::none();

        int moves_searched = 0;
        int legal_moves_count = 0;

        const Color us = pos.sideToMove;
        Move searched_quiets[MAX_MOVES];
        int quiet_count = 0;

        // Move iteration loop driven by MovePicker
        while (true)
        {
            const Move move = picker.next_move();
            if (!move.is_ok())
                break;

            const bool is_quiet = !move.is_capture() && !move.is_promotion();

            if constexpr (!IsPV)
            {
                if (is_null_window && !in_check && legal_moves_count > 0 && best_score > -MATE_BOUND)
                {
                    if (is_quiet)
                    {
                       
                        if (depth <= 4 && quiet_count >= (6 + 2 * depth * depth))
                            continue;

                        if (futility_prunable && move != current_killers[0] && move != current_killers[1] && move != counter)
                        {
                            continue;
                        }
                    }
                    else if (depth <= 3 && move.ordering_score() == MoveOrdering::BAD_CAPTURE)
                    {
                       
                        continue;
                    }
                }
            }

            const Piece moving_piece = pos.board[move.from()];

            // Lazy Legality Check: execute move and verify our King is not under attack
            pos.make_move(move);
            if (pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1)))
            {
                pos.unmake_move(move);
                continue; // Skip illegal move
            }
            legal_moves_count++;

            if (is_quiet)
            {
                searched_quiets[quiet_count++] = move;
            }

            if (ply < MAX_PLY)
            {
                path_moves[ply] = move;
                path_pieces[ply] = moving_piece;
            }

            int move_hist = 0;
            if (is_quiet)
            {
                move_hist = history_score(move, moving_piece);
                if (prev_piece < PIECE_NONE && is_valid(prev_to))
                {
                    move_hist = (move_hist + continuation_history_score(prev_piece, prev_to, moving_piece, move.to())) / 2;
                }
            }

            Value score = -VALUE_INFINITE;
            bool needs_full_search = true;

            int R = calculate_lmr_reduction(IsPV || !is_null_window, depth, moves_searched, in_check, move, current_killers,
                                            counter, move_hist);

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

                        if (score > alpha)
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

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    // Update best move at root level for UCI output
                    if constexpr (IsRoot)
                    {
                        ctx.info.best_move = move;
                    }

                    if constexpr (IsPV)
                    {
                        if (ply < MAX_PLY)
                        {
                            pv_table[ply][ply] = move;
                            const int child_len = (ply + 1 < MAX_PLY) ? pv_length[ply + 1] : (ply + 1);
                            for (int next_ply = ply + 1; next_ply < child_len; ++next_ply)
                            {
                                pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
                            }
                            pv_length[ply] = child_len;
                        }
                    }

                    // Beta Cutoff (Fail-High)
                    if (alpha >= beta)
                    {
                        if (is_quiet)
                        {
                            if (ply < MAX_PLY)
                                Search::update_killers(move, ply);

                            const int bonus = depth > 128 ? MAX_HISTORY_SCORE : depth * depth;
                            Search::update_history(move, depth, moving_piece);

                            if (prev_piece < PIECE_NONE && is_valid(prev_to))
                            {
                                Search::update_countermove(prev_piece, prev_to, move);
                                Search::update_continuation_history(prev_piece, prev_to, moving_piece, move.to(), bonus);
                            }

                            for (int i = 0; i < quiet_count - 1; ++i)
                            {
                                const Move qm = searched_quiets[i];
                                const Piece qp = pos.board[qm.from()];
                                Search::penalize_history(qm, depth, qp);
                                if (prev_piece < PIECE_NONE && is_valid(prev_to))
                                {
                                    Search::update_continuation_history(prev_piece, prev_to, qp, qm.to(), -bonus);
                                }
                            }
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
        Signals_stop.store(false, std::memory_order_relaxed);
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(path_moves, 0, sizeof(path_moves));
        std::memset(path_pieces, 0, sizeof(path_pieces));
        std::memset(pv_length, 0, sizeof(pv_length));
        age_history();
        TT.new_search();
        SearchContext ctx(limits, Signals_stop);

        const int64_t raw_time = (pos.sideToMove == WHITE) ? limits.wtime : limits.btime;
        const int64_t raw_inc = (pos.sideToMove == WHITE) ? limits.winc : limits.binc;
        const bool has_game_clock =
            !limits.infinite && (limits.wtime != 0 || limits.btime != 0 || limits.winc != 0 || limits.binc != 0);

        // If a game clock is active but raw_time <= 0 due to lag, clamp to 1ms instead of 0
        // so the engine plays immediately rather than entering an infinite UINT64_MAX search.
        const uint64_t time_left = (raw_time > 0) ? static_cast<uint64_t>(raw_time) : (has_game_clock ? 1ULL : 0ULL);
        const uint64_t inc = (raw_inc > 0) ? static_cast<uint64_t>(raw_inc) : 0ULL;

        if (limits.movetime > 0)
        {
            ctx.opt_time_ms = (limits.movetime > 20) ? static_cast<uint64_t>(limits.movetime - 20) : 1;
            ctx.max_time_ms = ctx.opt_time_ms;
        }
        else if (has_game_clock)
        {
            ctx.opt_time_ms = (time_left / 40) + (inc * 3 / 4);

            // Avoid sudden cliff jumps in safety margin calculations
            const uint64_t safe_margin = std::min(time_left / 4, static_cast<uint64_t>(50));

            // Cap max_time_ms closer to opt_time_ms instead of allowing an 8x gap
            ctx.max_time_ms = std::min(
                {time_left / 8, ctx.opt_time_ms * 3, (time_left > safe_margin ? time_left - safe_margin : time_left / 2)});

            if (ctx.max_time_ms == 0)
                ctx.max_time_ms = 1;

            ctx.opt_time_ms = std::clamp<uint64_t>(ctx.opt_time_ms, 1, ctx.max_time_ms);
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

        // Track base opt time and instability factor
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

            // Decay or compound opt_time_ms dynamically based on move stability at depth >= 5
            if (current_depth >= 5 && has_game_clock)
            {
                if (ctx.info.best_move == safe_best_move)
                {
                    time_extension_factor = std::max(1.0, time_extension_factor * 0.9); // Decay
                }
                else
                {
                    time_extension_factor = std::min(2.5, time_extension_factor + 0.5); // Compound
                }
                ctx.opt_time_ms = std::min(static_cast<uint64_t>(base_opt_time_ms * time_extension_factor), ctx.max_time_ms);
            }

            safe_best_move = ctx.info.best_move;
            prev_score = score;
            ctx.info.score = score;
            ctx.info.time_ms = elapsed;

            ctx.info.pv.clear();
            for (int i = 0; i < pv_length[0]; ++i)
            {
                if (pv_table[0][i].is_ok())
                {
                    ctx.info.pv.push_back(pv_table[0][i]);
                }
            }
            if (ctx.info.pv.empty() && safe_best_move.is_ok())
            {
                ctx.info.pv.push_back(safe_best_move);
            }

            // Output UCI telemetry progress update
            UCI::send_search_info(ctx.info);

            if (has_game_clock)
            {
                if (elapsed >= static_cast<uint64_t>(ctx.opt_time_ms * 0.65))
                    break;
            }
            else if ( elapsed >= ctx.opt_time_ms )
            {
                break;
            }

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