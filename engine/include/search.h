#pragma once

#include "position.h"
#include "types.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace Search
{
    /**
     * @brief Node types for template-specialized search dispatching.
     */
    enum NodeType
    {
        ROOT,
        PV,
        NON_PV
    };

    /**
     * @brief Flag to asynchronously interrupt the search (e.g., UCI 'stop' or time up).
     */
    class HistoryTable;
    class TranspositionTable;

    /** @brief Global atomic flag used to interrupt search thread asynchronously (e.g., UCI 'stop' or time out). */
    extern std::atomic<bool> Signals_stop;

    /**
     * @brief Live search telemetry for UCI updates.
     */
    struct SearchInfo
    {
        int depth = 0;                 /**< Completed search depth. */
        int seldepth = 0;              /**< Max selective depth (q-search). */
        uint64_t nodes = 0;            /**< Total nodes evaluated. */
        uint64_t time_ms = 0;          /**< Search time in milliseconds. */
        Value score = VALUE_ZERO;      /**< Current best score (centipawns or mate). */
        Move best_move = Move::none(); /**< Best move found so far. */
        std::vector<Move> pv;          /**< Principal Variation (expected best line). */
    };

    struct SearchContext
    {
        const SearchLimits& limits;
        SearchInfo info{};
        std::atomic<bool>& stop_flag;

        // Time tracking
        std::chrono::steady_clock::time_point start_time;
        uint64_t time_limit_ms = 0;

        HistoryTable* history_table = nullptr;

        // Optional Transposition Table for caching previously evaluated positions
        TranspositionTable* tt = nullptr;

        SearchContext(const SearchLimits& l, std::atomic<bool>& stop)
            : limits(l),
              stop_flag(stop),
              start_time(std::chrono::steady_clock::now())
        {
        }
    };

    /**
     * @brief Clears search heuristics (history, killers, etc.) before a new search.
     */
    void init();

    /**
     * @brief Precomputes the baseline Late Move Reduction values using a logarithmic curve.
     *        Called once during engine startup.
     */
    void init_lmr_table();

    /**
     * @brief Primary engine execution loop implementing Iterative Deepening.
     *
     * Launched by the UCI thread to execute search within specified constraints.
     * Emits search depth, node counts, evaluation scores, and bestmove to standard output.
     *
     * @param pos Reference to the current starting position state.
     * @param limits Const reference to UCI search parameters (depth, time, movetime).
     */
    void start_search(Position& pos, const SearchLimits& limits);

    /**
     * @brief Recursive template-based Alpha-Beta (Negamax) search algorithm.
     *
     * Uses compile-time template parameter dispatching (ROOT, PV, NON_PV) to strip
     * unreachable branch conditions at compile time, eliminating CPU branch mispredictions.
     *
     * @tparam NT Node type designation (ROOT, PV, or NON_PV).
     * @param pos Reference to the active board position state.
     * @param depth Remaining depth to search in half-moves.
     * @param ply Current depth distance from the root node.
     * @param alpha Lower bound score (best guaranteed score for maximizing player).
     * @param beta Upper bound score (worst score the minimizing player allows).
     * @param ctx Execution parameters, telemetry counters, and heuristic tables.
     * @param do_null Flag permitting Null-Move Pruning at the current branch node.
     * @return Value Computed evaluation score relative to the side to move.
     */
    template <NodeType NT>
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, SearchContext& ctx, bool do_null = true);

    /**
     * @brief Evaluates tactical captures at leaf nodes to prevent the horizon effect.
     *
     * @param pos Current board position.
     * @param ply Distance from the root node.
     * @param alpha Lower bound of the search window.
     * @param beta Upper bound of the search window.
     * @return Stable, tactically quiet score.
     */
    Value quiescence(Position& pos, int ply, Value alpha, Value beta, SearchContext& ctx);

    /**
     * @brief Attempts Null-Move Pruning (NMP) to aggressively prune the search tree.
     *
     * @param pos Current board position.
     * @param depth Current search depth.
     * @param ply Distance from root.
     * @param beta Upper bound for the cutoff check.
     * @param ctx Execution context and telemetry.
     * @param out_score Reference populated with the cutoff score if pruning succeeds.
     * @return true if a beta cutoff is achieved, false otherwise.
     */
    inline bool try_null_move_pruning(Position& pos, int depth, int ply, Value beta, SearchContext& ctx, Value& out_score);

} // namespace Search