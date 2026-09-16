#pragma once

#include "types.h"
#include <cstdint>
#include <vector>

namespace Search
{
    /**
     * @brief Node evaluation bounds for Alpha-Beta pruning.
     */
    enum TTFlag : uint8_t
    {
        TT_EXACT,      /**< Exact score (PV node). */
        TT_LOWERBOUND, /**< Score caused a beta cutoff (fail-high). */
        TT_UPPERBOUND  /**< Score didn't beat alpha (fail-low). */
    };

    /**
     * @brief A single entry in the Transposition Table.
     *        Packed to minimize memory footprint.
     */
    struct TTEntry
    {
        Key key = 0;                   /**< 64-bit Zobrist hash to verify position collisions. */
        Move best_move = Move::none(); /**< Best move found in this position. */
        Value score = VALUE_ZERO;      /**< Evaluated score. */
        int depth = 0;                 /**< Search depth at which this score was calculated. */
        TTFlag flag = TT_EXACT;        /**< Node evaluation type (Exact, Lower, Upper). */
    };

    /**
     * @brief The global transposition table (Hash Table).
     *        Draft implementation using a flat array and modulo mapping.
     */
    class TranspositionTable
    {
    public:
        TranspositionTable() = default;
        ~TranspositionTable() = default;

        /**
         * @brief Allocates memory for the TT.
         *
         * @param mb Requested size in megabytes.
         */
        void resize(int mb);

        /**
         * @brief Clears all entries in the table.
         */
        void clear();

        /**
         * @brief Saves a position's evaluation to the cache.
         *        Currently uses a simple depth-preferred replacement strategy.
         *
         * @param key Zobrist key of the position.
         * @param depth Depth searched.
         * @param score Evaluated score.
         * @param flag Bound type (Exact, Lower, Upper).
         * @param move Best move found.
         */
        void store(Key key, int depth, Value score, TTFlag flag, Move move);

        /**
         * @brief Attempts to retrieve a cached evaluation for the current position.
         *
         * @param key Zobrist key to look up.
         * @param out_entry Reference to populate if a valid match is found.
         * @return True if a valid match was found, false otherwise.
         */
        bool probe(Key key, TTEntry& out_entry) const;

        /**
         * @brief Returns how full the TT is (per thousand entries).
         *        Used for UCI 'hashfull' telemetry.
         */
        int hashfull() const;

    private:
        std::vector<TTEntry> table;
        uint64_t entry_count = 0; /**< Tracks total occupied slots for telemetry. */
    };

    /**
     * @brief Global TT instance to be shared across the search engine.
     */
    extern TranspositionTable TT;

} // namespace Search