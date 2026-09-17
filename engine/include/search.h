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

        // Hooks for Engineer 2 (Move Ordering)
        Move killer_moves[MAX_PLY][2]{};
        HistoryTable* history_table = nullptr;

        // Hooks for Engineer 3 (Caching)
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
     * @brief Root entry point for Iterative Deepening. Launched by the UCI thread.
     *
     * @param pos Current board position.
     * @param limits UCI time and depth constraints.
     */
    void start_search(Position& pos, const SearchLimits& limits);

    /**
     * @brief Core recursive template-based alpha-beta search.
     *
     * @tparam NT Node type (ROOT, PV, or NON_PV) for compile-time branch optimization.
     * @param pos Current board position.
     * @param depth Remaining search depth.
     * @param ply Distance from the root node.
     * @param alpha Lower bound of the search window.
     * @param beta Upper bound of the search window.
     * @param ctx Execution context and heuristics state.
     * @param do_null Permits Null-Move Pruning at this node.
     * @return Evaluated score for the position.
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

} // namespace Search