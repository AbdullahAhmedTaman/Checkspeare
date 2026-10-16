#include "transposition.h"
#include <algorithm>
#include <cstring>
namespace Search
{
    TranspositionTable TT;

    void TranspositionTable::resize(int mb)
    {
        if (mb <= 0)
            mb = 1; // Minimum size of 1MB
        // Calculate maximum 64-byte buckets that fit in the requested memory.
        uint64_t bytes = static_cast<uint64_t>(mb) * 1024 * 1024;
        uint64_t target_buckets = bytes / sizeof(TTBucket);

        // Isolate the largest power of 2 that is <= target_buckets.
        uint64_t num_buckets = 1;
        while ((num_buckets * 2) <= target_buckets)
        {
            num_buckets *= 2;
        }

        // Generate bitmask for division-less indexing (e.g., 000011111111).
        bucket_mask = num_buckets - 1;

        table.resize(num_buckets);
        clear();
    }

    void TranspositionTable::clear()
    {
        if (!table.empty())
        {
            std::memset(table.data(), 0, table.size() * sizeof(TTBucket));
        }    
    }

    void TranspositionTable::store(Key key, int ply, int depth, Value score, TTFlag flag, Move move)
    {
        if (table.empty())
            return;

        if (score >= MATE_BOUND)
            score += ply;
        else if (score <= -MATE_BOUND)
            score -= ply;

        TTBucket& bucket = table[key & bucket_mask];
       int match_slot = -1;
        int empty_slot = -1;
        int worst_slot = 0;
        int worst_score = -999999;

        for (int i = 0; i < 4; ++i)
        {
            const TTEntry& entry = bucket.entries[i];
            if (entry.key == key)
            {
                match_slot = i;
                break;
            }
            if (entry.key == 0 && empty_slot == -1)
            {
                empty_slot = i;
            }

            // Calculate how old the entry is (0 = current search, >0 = older searches)
            const int age_diff = (global_age - entry.age() + 64) & 0x3F;
            const int replace_score = (age_diff * 256) - entry.depth;
            if (replace_score > worst_score)
            {
                worst_score = replace_score;
                worst_slot = i;
            }
        }

        const bool found_key = (match_slot != -1);
        const int target_slot = found_key ? match_slot : (empty_slot != -1 ? empty_slot : worst_slot);
        TTEntry& target = bucket.entries[target_slot];

        move.set_score(0);
        const bool has_new_move = move.is_ok() && !move.is_null();

        // Don't overwrite deeper entries from the SAME generation
        if (found_key && target.age() == global_age && depth < target.depth)
        {
            // Still salvage the move hint if the existing entry had none and this is not a fail-low
            if (!target.best_move.is_ok() && has_new_move && flag != TT_UPPERBOUND)
            {
                target.best_move = move;
            }
            return;
        }

        const Move preserved_move = found_key ? target.best_move : Move::none();

        target.key = key;
        target.depth = static_cast<uint8_t>(std::clamp(depth, 0, 255));
        target.score = static_cast<int16_t>(score);
        target.pack_gen_bound(global_age, flag);

        // Only overwrite an existing best_move if the new move comes from an EXACT or LOWERBOUND
        // node, or if there was no prior best_move stored for this key.
        if (has_new_move && (flag != TT_UPPERBOUND || !preserved_move.is_ok()))
        {
            target.best_move = move;
        }
        else
        {
            target.best_move = preserved_move;
        }
    }

    bool TranspositionTable::probe(Key key, int ply, TTEntry& out_entry) const
    {
        if (table.empty())
            return false;

        const TTBucket& bucket = table[key & bucket_mask];

        for (int i = 0; i < 4; ++i)
        {
            if (bucket.entries[i].key == key)
            {
                out_entry = bucket.entries[i];

                // Decouple the stored score from the table's depth to match the current search ply
                if (out_entry.score >= MATE_BOUND)
                    out_entry.score -= ply;
                else if (out_entry.score <= -MATE_BOUND)
                    out_entry.score += ply;

                return true;
            }
        }
        return false; // Cache miss
    }

    int TranspositionTable::hashfull() const
    {
        if (table.empty())
            return 0;

        int sampled_entries = 0;
        int filled_entries = 0;
        int max_samples = std::min(static_cast<int>(table.size()), 1000);

        for (int i = 0; i < max_samples; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                sampled_entries++;
                if (table[i].entries[j].key != 0)
                {
                    filled_entries++;
                }
            }
        }

        if (sampled_entries == 0)
            return 0;
        return (filled_entries * 1000) / sampled_entries;
    }

} // namespace Search