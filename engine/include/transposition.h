#pragma once

#include "types.h"
#include <cstdint>
#include <vector>

namespace Search
{

    constexpr int MAX_SEARCH_PLY = 256;
    /**
     * @brief Threshold to detect forced mate scores.
     *        Any score beyond +/- this bound is a mate sequence.
     */

    constexpr Value MATE_BOUND = VALUE_MATE - MAX_SEARCH_PLY;

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
     * @brief 16-byte packed Transposition Table entry.
     *        Explicit alignment prevents memory boundary crossing in the CPU cache.
     */
    struct alignas(16) TTEntry
    {
        Key key = 0;                   /**< 64-bit Zobrist hash (8 bytes). */
        Move best_move = Move::none(); /**< Best move found (2 bytes). */
        int16_t score = 0;             /**< Evaluated score (2 bytes). */
        int16_t depth = 0;             /**< Search depth (2 bytes). */
        TTFlag flag = TT_EXACT;        /**< Node evaluation type (1 byte). */
        // 2 bytes of implicit compiler padding to reach exactly 16 bytes.
    };

    /**
     * @brief 64-byte bucket matching a standard CPU L1 cache line.
     *        Contains 4 entries for lockless collision resolution.
     */
    struct alignas(64) TTBucket
    {
        TTEntry entries[4];
    };

    /**
     * @brief The global transposition table (Hash Table).
     *        Utilizes power-of-two bitmask indexing and 4-way associativity.
     */
    class TranspositionTable
    {
    public:
        TranspositionTable() = default;
        ~TranspositionTable() = default;

        /**
         * @brief Allocates memory for the TT based on a strict power-of-two size.
         *
         * @param mb Requested size in megabytes, typically 64MB-256MB.
         */
        void resize(int mb);

        /**
         * @brief Clears all entries in the table by zeroing memory.
         */
        void clear();

        /**
         * @brief Saves a position's evaluation to the cache.
         *        Utilizes manual loop unrolling and a branchless Two-Tier replacement strategy.
         *
         * @param key Zobrist key of the position.
         * @param depth Depth searched.
         * @param score Evaluated score.
         * @param flag Bound type (Exact, Lower, Upper).
         * @param move Best move found.
         */
        void store(Key key, int ply, int depth, Value score, TTFlag flag, Move move);

        /**
         * @brief Attempts to retrieve a cached evaluation for the current position.
         *        Executes a manually unrolled, single cache-line fetch.
         *
         * @param key Zobrist key to look up.
         * @param out_entry Reference to populate if a valid match is found.
         * @return True if a valid match was found, false otherwise.
         */
        bool probe(Key key, int ply, TTEntry& out_entry) const;

        /**
         * @brief Samples the table to determine how full it is (per thousand entries).
         *        Required for UCI 'hashfull' telemetry.
         */
        int hashfull() const;

    private:
        std::vector<TTBucket> table;
        uint64_t bucket_mask = 0; /**< Bitmask for fast O(1) index calculation. */
    };

    /**
     * @brief Global TT instance to be shared across the search engine.
     */
    extern TranspositionTable TT;

} // namespace Search