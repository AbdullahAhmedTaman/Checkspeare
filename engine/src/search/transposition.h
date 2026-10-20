#pragma once

#include "board/types.h"
#include <cstdint>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace Search
{

    constexpr int DEFAULT_HASH_MB = 64;
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
     *
     * Exact 16-byte layout alignment:
     *   Offset  0: Key key             (8 bytes)
     *   Offset  8: Move best_move      (4 bytes - 32-bit packed)
     *   Offset 12: int16_t score       (2 bytes)
     *   Offset 14: uint8_t depth       (1 byte: supports plies 0..255)
     *   Offset 15: uint8_t gen_bound   (1 byte: Bits 0-1: TTFlag, Bits 2-7: Age)
     *   Total: exactly 16 bytes.
     */
    struct alignas(16) TTEntry
    {
        Key key = 0;
        Move best_move = Move::none();
        int16_t score = 0;
        uint8_t depth = 0;
        uint8_t gen_bound = 0; // Bits 0-1: TTFlag, Bits 2-7: Age

        // Inline accessors for the packed byte
        [[nodiscard]] inline TTFlag flag() const
        {
            return static_cast<TTFlag>(gen_bound & 0x3);
        }

        [[nodiscard]] inline uint8_t age() const
        {
            return gen_bound >> 2;
        }

        inline void pack_gen_bound(uint8_t global_age, TTFlag f)
        {
            gen_bound = static_cast<uint8_t>((global_age << 2) | (f & 0x3));
        }
    };

    static_assert(sizeof(TTEntry) == 16, "TTEntry must remain exactly 16 bytes");

    /**
     * @brief 64-byte bucket matching a standard CPU L1 cache line.
     *        Contains 4 entries for lockless collision resolution.
     */
    struct alignas(64) TTBucket
    {
        TTEntry entries[4];
    };

    static_assert(sizeof(TTBucket) == 64, "TTBucket must remain exactly 64 bytes");

    /**
     * @brief The global transposition table (Hash Table).
     *        Utilizes power-of-two bitmask indexing and 4-way associativity.
     */
    class TranspositionTable
    {
    public:
        TranspositionTable()
        {
            resize(DEFAULT_HASH_MB);
        }

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
         * @param ply Root-relative ply used for mate-score normalization.
         * @param depth Depth searched in plies.
         * @param score Evaluated score.
         * @param flag Bound type (exact, lower, or upper).
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

        // Bumps the generation counter (wraps at 63 since it's 6-bit)
        void new_search()
        {
            global_age = (global_age + 1) & 0x3F;
        }

        /**
         * @brief Prefetches the 64-byte TT bucket for the given Zobrist key into L1 cache.
         *
         * Called early in negamax before in_check() so the memory controller fetches
         * the cache line in parallel with attack bitboard calculations.
         */
        inline void prefetch(Key key) const noexcept
        {
            if (table.empty())
                return;
            const void* addr = &table[key & bucket_mask];
#if defined(__x86_64__) || defined(_M_X64)
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(addr, 0, 3);
#endif
        }

    private:
        std::vector<TTBucket> table;
        uint64_t bucket_mask = 0; /**< Bitmask for fast O(1) index calculation. */
        uint8_t global_age = 0;   /**< Track the current search generation. */
    };

    /**
     * @brief Global TT instance to be shared across the search engine.
     */
    extern TranspositionTable TT;

} // namespace Search