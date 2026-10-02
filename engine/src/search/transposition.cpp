#include "transposition.h"
#include <algorithm>

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
        std::fill(table.begin(), table.end(), TTBucket{});
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
        int target_slot = -1;
        bool found_key = false;

        for (int i = 0; i < 4; ++i)
        {
            if (bucket.entries[i].key == key)
            {
                target_slot = i;
                found_key = true;
                break;
            }
        }

        if (!found_key)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (bucket.entries[i].key == 0)
                {
                    target_slot = i;
                    break;
                }
            }
        }

        if (target_slot == -1)
        {
            int worst_score = -999999;
            for (int i = 0; i < 4; ++i)
            {
                // Calculate how old the entry is (0 = current search, >0 = older searches)
                int age_diff = (global_age - bucket.entries[i].age() + 64) & 0x3F;

                // Eviction Priority:
                // Always replace older searches before current searches (age_diff * 256).
                // If age is tied, replace the entry with the shallowest depth (-depth).
                int replace_score = (age_diff * 256) - bucket.entries[i].depth;

                if (replace_score > worst_score)
                {
                    worst_score = replace_score;
                    target_slot = i;
                }
            }
        }

        // Don't overwrite deeper entries from the SAME generation
        if (found_key && bucket.entries[target_slot].age() == global_age && depth < bucket.entries[target_slot].depth)
        {
            return;
        }

        Move preserved_move = Move::none();
        if (found_key)
            preserved_move = bucket.entries[target_slot].best_move;

        move.set_score(0);

        bucket.entries[target_slot].key = key;
        bucket.entries[target_slot].depth = static_cast<uint8_t>(std::clamp(depth, 0, 255));
        bucket.entries[target_slot].score = static_cast<int16_t>(score);
        bucket.entries[target_slot].pack_gen_bound(global_age, flag);

        if (move.is_ok() && !move.is_null())
        {
            bucket.entries[target_slot].best_move = move;
        }
        else
        {
            bucket.entries[target_slot].best_move = preserved_move;
        }
    }

    bool TranspositionTable::probe(Key key, int ply, TTEntry& out_entry) const
    {
        if (table.empty())
            return false;

        const TTBucket& bucket = table[key & bucket_mask];

        if (bucket.entries[0].key == key)
        {
            out_entry = bucket.entries[0];
        }
        else if (bucket.entries[1].key == key)
        {
            out_entry = bucket.entries[1];
        }
        else if (bucket.entries[2].key == key)
        {
            out_entry = bucket.entries[2];
        }
        else if (bucket.entries[3].key == key)
        {
            out_entry = bucket.entries[3];
        }
        else
        {
            return false;
        } // Cache miss

        // Decouple the stored score from the table's depth to match the current search ply
        if (out_entry.score >= MATE_BOUND)
            out_entry.score -= ply;
        else if (out_entry.score <= -MATE_BOUND)
            out_entry.score += ply;

        return true;
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