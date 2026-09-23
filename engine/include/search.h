#pragma once

#include "position.h"
#include "types.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace Search
{
    /**
     * @brief Flag to asynchronously interrupt the search (e.g., UCI 'stop' or time up).
     */
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

    /** @brief Search telemetry updated by the active search. */
    extern SearchInfo CurrentInfo;

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
     * @brief Core recursive alpha-beta search.
     *
     * @param pos Current board position.
     * @param depth Remaining search depth.
     * @param ply Distance from the root node.
     * @param alpha Lower bound of the search window.
     * @param beta Upper bound of the search window.
     * @param do_null Permits Null-Move Pruning at this node.
     * @return Evaluated score for the position.
     */
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool do_null = true);

    /**
     * @brief Evaluates tactical captures at leaf nodes to prevent the horizon effect.
     *
     * @param pos Current board position.
     * @param ply Distance from the root node.
     * @param alpha Lower bound of the search window.
     * @param beta Upper bound of the search window.
     * @return Stable, tactically quiet score.
     */
    Value quiescence(Position& pos, int ply, Value alpha, Value beta);

} // namespace Search