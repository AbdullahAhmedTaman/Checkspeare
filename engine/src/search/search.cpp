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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

bool is_repetition(const Position& pos);

namespace Search
{
    Move killer_moves[MAX_PLY][2];
    int history_table[MAX_PIECES][MAX_SQUARES];

    // Nodes spent below each root move during the current iteration (indexed [from][to]).
    static uint64_t root_move_nodes[MAX_SQUARES][MAX_SQUARES];

    // UCI configurable safety margin for GUI/network latency
    int move_overhead_ms = 30;

    //  Time Management
    namespace
    {
        constexpr uint64_t NO_LIMIT = UINT64_MAX;
        constexpr int DEFAULT_MOVES_TO_GO = 40;
        constexpr int DYNAMIC_TM_MIN_DEPTH = 6;

        constexpr int ASPIRATION_MIN_DEPTH = 5;
        constexpr int ASPIRATION_FULL_DELTA = 300;

        inline uint64_t elapsed_ms(const SearchContext& ctx)
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ctx.start_time).count());
        }

        struct TimeBudget
        {
            uint64_t optimum = NO_LIMIT; // Soft limit: checked between iterations and scaled dynamically
            uint64_t maximum = NO_LIMIT; // Hard limit: aborts search mid-iteration
            bool dynamic = false;        // False for fixed movetime, depth, or infinite
        };

        TimeBudget allocate_time(const Position& pos, const SearchLimits& limits)
        {
            TimeBudget tb;
            const int64_t overhead = std::max<int64_t>(0, move_overhead_ms);

            if (limits.movetime > 0)
            {
                const int64_t slice = static_cast<int64_t>(limits.movetime) - overhead;
                tb.optimum = tb.maximum = static_cast<uint64_t>(std::max<int64_t>(slice, 1));
                return tb;
            }

            if (limits.wtime <= 0 && limits.btime <= 0)
                return tb; // Infinite or depth-limited search

            const bool white = (pos.sideToMove == WHITE);
            const int64_t time_left = std::max<int64_t>(static_cast<int64_t>(white ? limits.wtime : limits.btime), 1);
            const int64_t inc = std::max<int64_t>(static_cast<int64_t>(white ? limits.winc : limits.binc), 0);

            // Reserve buffer so latency overhead doesn't consume all remaining time
            const int64_t usable = std::max<int64_t>({time_left - overhead, time_left / 4, static_cast<int64_t>(1)});

            const int mtg = DEFAULT_MOVES_TO_GO;

            const double base = static_cast<double>(usable) / mtg + 0.75 * static_cast<double>(inc);
            const double opt_cap = 0.50;

            const double optimum = std::min(base, opt_cap * static_cast<double>(usable));
            const double maximum = std::min(0.80 * static_cast<double>(usable), optimum * 5.0);

            tb.optimum = std::max<uint64_t>(static_cast<uint64_t>(optimum), 1);
            tb.maximum = std::max<uint64_t>(static_cast<uint64_t>(maximum), tb.optimum);
            tb.dynamic = true;
            return tb;
        }

        double dynamic_time_scale(int depth, int stability, Value score, Value prev_score, int fail_lows, double best_move_effort)
        {
            if (depth < DYNAMIC_TM_MIN_DEPTH)
                return 1.0;

            // 1) Move stability scaling
            static constexpr double STABILITY[6] = {1.50, 1.25, 1.05, 0.92, 0.82, 0.75};
            double scale = STABILITY[std::min(stability, 5)];

            // 2) Score trend scaling (spend extra time if position evaluation is falling)
            if (std::abs(score) < MATE_BOUND && std::abs(prev_score) < MATE_BOUND)
            {
                const double drop = static_cast<double>(prev_score) - static_cast<double>(score);
                scale *= std::clamp(1.0 + 0.006 * drop, 0.85, 1.60);
            }

            // 3) Aspiration fail-low scaling
            scale *= 1.0 + 0.12 * std::min(fail_lows, 3);

            // 4) Effort scaling (easier decision if most nodes were spent on one root move)
            scale *= std::clamp(1.65 - best_move_effort, 0.65, 1.40);

            return std::clamp(scale, 0.40, 3.00);
        }

        inline int initial_aspiration_delta(Value prev_score)
        {
            const int64_t s = static_cast<int64_t>(prev_score);
            return 10 + static_cast<int>((s * s) / 15000);
        }
    } // namespace

    // =====================================================================
    //  Pruning Helpers & Setup
    // =====================================================================
    bool try_null_move_pruning(Position& pos, int depth, int ply, Value beta, SearchContext& ctx, Value& out_score)
    {
        Value static_eval = Eval::evaluate(pos);
        Color us = pos.sideToMove;

        Bitboard non_pawn_material =
            pos.pieces[us][KNIGHT] | pos.pieces[us][BISHOP] | pos.pieces[us][ROOK] | pos.pieces[us][QUEEN];

        if (static_eval >= beta && non_pawn_material != 0)
        {
            int R = 2 + (depth / 4);

            pos.make_move(Move::null());
            Value null_score = -negamax<NON_PV>(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, ctx, false);
            pos.unmake_move(Move::null());

            if (null_score >= beta)
            {
                out_score = (null_score >= MATE_BOUND) ? beta : null_score;
                return true;
            }
        }

        return false;
    }

    int LMRTable[MAX_PLY][MAX_MOVES];

    void init_lmr_table()
    {
        for (int depth = 0; depth < MAX_PLY; ++depth)
        {
            for (int moves = 0; moves < MAX_MOVES; ++moves)
            {
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
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(history_table, 0, sizeof(history_table));
        std::memset(root_move_nodes, 0, sizeof(root_move_nodes));
    }

    inline int calculate_lmr_reduction(bool is_pv, int depth, int moves_searched, bool in_check, const Move& move,
                                       const Move* killers)
    {
        if (depth < 3 || moves_searched < 3 || in_check || move.is_capture() || move.is_promotion())
        {
            return 0;
        }

        int safe_depth = std::min(depth, MAX_PLY - 1);
        int safe_moves = std::min(moves_searched, MAX_MOVES - 1);
        int R = LMRTable[safe_depth][safe_moves];

        if (is_pv)
            R -= 1;

        if (move == killers[0] || move == killers[1])
            R -= 1;

        return std::clamp(R, 0, depth - 1);
    }

    // =====================================================================
    //  Core Negamax Search Algorithm
    // =====================================================================
    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null)
    {
        if (ply >= MAX_PLY - 1)
            return Eval::evaluate(pos);

        // Mate Distance Pruning
        alpha = std::max(alpha, -VALUE_MATE + ply);
        beta = std::min(beta, VALUE_MATE - ply - 1);
        Value alpha_orig = alpha;

        if (alpha >= beta)
            return alpha;

        constexpr bool IsPV = (NT != NON_PV);
        constexpr bool IsRoot = (NT == ROOT);

        if (Signals_stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Draw Detection
        if (ply > 0 && (pos.halfmoveClock >= 100 || is_repetition(pos)))
        {
            return VALUE_ZERO;
        }

        bool in_check = pos.in_check();

        if (in_check && ply < MAX_PLY - 1)
        {
            depth++; // Check extension
        }

        if (depth <= 0)
        {
            return quiescence(pos, ply, alpha, beta, ctx);
        }

        ctx.info.nodes++;

        // Polling wall clock (hard time ceiling enforcement)
        if ((ctx.info.nodes & 2047) == 0 && ctx.max_time_ms != NO_LIMIT)
        {
            if (elapsed_ms(ctx) >= ctx.max_time_ms)
            {
                Signals_stop.store(true, std::memory_order_relaxed);
            }
        }

        // Null Move Pruning
        if constexpr (!IsPV)
        {
            if (do_null && depth >= 3 && !in_check)
            {
                Value nmp_score;
                if (try_null_move_pruning(pos, depth, ply, beta, ctx, nmp_score))
                {
                    return nmp_score;
                }
            }
        }

        // Transposition Table Probe
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

        static const Move empty_killers[2] = {Move::none(), Move::none()};
        const Move* current_killers = (ply < MAX_PLY) ? Search::killer_moves[ply] : empty_killers;

        // Move Generation & MovePicker Initialization
        MoveList moves;
        MoveGen::generate_legal_moves(pos, moves);

        if (moves.count == 0)
        {
            return in_check ? (-VALUE_MATE + ply) : VALUE_ZERO;
        }

        MoveOrdering::MovePicker picker(moves, pos, tt_move, ply);

        Value best_score = -VALUE_INFINITE;
        Move best_move = Move::none();
        int moves_searched = 0;

        Move searched_quiets[MAX_MOVES];
        int quiet_count = 0;

        while (true)
        {
            const Move move = picker.next_move();
            if (!move.is_ok())
                break;

            bool is_quiet = !move.is_capture() && !move.is_promotion();
            if (is_quiet)
            {
                searched_quiets[quiet_count++] = move;
            }

            pos.make_move(move);

            Value score = -VALUE_INFINITE;
            [[maybe_unused]] const auto nodes_before = ctx.info.nodes;
            bool needs_full_search = true;

            int R = calculate_lmr_reduction(IsPV, depth, moves_searched, in_check, move, current_killers);
            if (R > 0)
            {
                Value reduced_score = -negamax<NON_PV>(pos, depth - 1 - R, ply + 1, -alpha - 1, -alpha, ctx, true);
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

            if constexpr (IsRoot)
            {
                root_move_nodes[move.from()][move.to()] += ctx.info.nodes - nodes_before;
            }

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
                        if (is_quiet)
                        {
                            if (ply < MAX_PLY)
                                Search::update_killers(move, ply);

                            Search::update_history(move, depth, pos.board[move.from()]);

                            // Apply history gravity penalties to failed quiet moves
                            for (int i = 0; i < quiet_count - 1; ++i)
                            {
                                Search::penalize_history(searched_quiets[i], depth, pos.board[searched_quiets[i].from()]);
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (!Signals_stop.load(std::memory_order_relaxed))
        {
            TTFlag flag = (best_score >= beta) ? TT_LOWERBOUND : (best_score > alpha_orig) ? TT_EXACT : TT_UPPERBOUND;
            TT.store(pos.key, ply, depth, best_score, flag, best_move);
        }

        return best_score;
    }

    //  Iterative Deepening Control Loop
    void start_search(Position& pos, const SearchLimits& limits)
    {
        init();
        TT.clear();
        SearchContext ctx(limits, Signals_stop);

        const TimeBudget budget = allocate_time(pos, limits);
        ctx.opt_time_ms = budget.optimum;
        ctx.max_time_ms = budget.maximum;

        Move safe_best_move = Move::none();

        MoveList root_moves;
        MoveGen::generate_legal_moves(pos, root_moves);
        if (root_moves.count > 0)
            safe_best_move = root_moves.moves[0];

        // Single move safeguard
        if (root_moves.count == 1 && budget.maximum != NO_LIMIT)
        {
            std::cout << "bestmove " << safe_best_move << std::endl;
            return;
        }

        Value prev_score = VALUE_ZERO;
        Move prev_best = Move::none();
        int stability = 0;

        for (int current_depth = 1; current_depth <= limits.depth; ++current_depth)
        {
            if (Signals_stop.load(std::memory_order_relaxed))
                break;

            ctx.info.depth = current_depth;
            std::memset(root_move_nodes, 0, sizeof(root_move_nodes));
            const auto iter_start_nodes = ctx.info.nodes;

            Value score = VALUE_ZERO;
            int fail_lows = 0;
            Move pending_best = Move::none();

            const bool use_aspiration = current_depth >= ASPIRATION_MIN_DEPTH && std::abs(prev_score) < MATE_BOUND;

            if (!use_aspiration)
            {
                ctx.info.best_move = Move::none();
                score = negamax<ROOT>(pos, current_depth, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
            }
            else
            {
                int delta = initial_aspiration_delta(prev_score);
                int alpha = std::max<int>(static_cast<int>(prev_score) - delta, -VALUE_INFINITE);
                int beta = std::min<int>(static_cast<int>(prev_score) + delta, VALUE_INFINITE);

                while (true)
                {
                    ctx.info.best_move = Move::none();
                    score = negamax<ROOT>(pos, current_depth, 0, static_cast<Value>(alpha), static_cast<Value>(beta), ctx, false);

                    if (Signals_stop.load(std::memory_order_relaxed))
                        break;

                    if (alpha <= -VALUE_INFINITE && beta >= VALUE_INFINITE)
                        break;

                    if (score <= alpha)
                    {
                        ++fail_lows;
                        beta = (alpha + beta) / 2;
                        alpha = std::max<int>(static_cast<int>(score) - delta, -VALUE_INFINITE);
                    }
                    else if (score >= beta)
                    {
                        pending_best = ctx.info.best_move;

                        // Soft-time check during fail-high: early exit using valid candidate
                        if (elapsed_ms(ctx) >= ctx.opt_time_ms && pending_best != Move::none())
                            break;

                        beta = std::min<int>(static_cast<int>(score) + delta, VALUE_INFINITE);
                    }
                    else
                    {
                        break;
                    }

                    delta += delta;
                    if (delta >= ASPIRATION_FULL_DELTA)
                    {
                        alpha = -VALUE_INFINITE;
                        beta = VALUE_INFINITE;
                    }
                }
            }

            if (Signals_stop.load(std::memory_order_relaxed))
            {
                if (ctx.info.best_move != Move::none())
                    safe_best_move = ctx.info.best_move;
                else if (pending_best != Move::none())
                    safe_best_move = pending_best;
                break;
            }

            const uint64_t elapsed = elapsed_ms(ctx);
            const Move best = ctx.info.best_move;

            if (best != Move::none())
                safe_best_move = best;

            stability = (current_depth > 1 && best == prev_best) ? stability + 1 : 0;
            prev_best = best;

            // Recalculate dynamic soft limit based on iteration search effort
            if (budget.dynamic)
            {
                double effort = 0.5;
                const uint64_t iter_nodes = ctx.info.nodes - iter_start_nodes;
                if (best != Move::none() && iter_nodes > 0)
                {
                    effort = std::min(1.0, static_cast<double>(root_move_nodes[best.from()][best.to()]) /
                                               static_cast<double>(iter_nodes));
                }

                const double scale = dynamic_time_scale(current_depth, stability, score, prev_score, fail_lows, effort);
                ctx.opt_time_ms = static_cast<uint64_t>(
                    std::min(static_cast<double>(budget.optimum) * scale, static_cast<double>(budget.maximum)));
            }

            prev_score = score;
            ctx.info.score = score;
            ctx.info.time_ms = elapsed;

            UCI::send_search_info(ctx.info);

            if (elapsed >= ctx.opt_time_ms)
                break;

            if (std::abs(score) >= MATE_BOUND && current_depth >= (VALUE_MATE - std::abs(score)))
                break;
        }

        std::cout << "bestmove " << safe_best_move << std::endl;
    }

    template Value negamax<ROOT>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);
    template Value negamax<NON_PV>(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null);

} // namespace Search