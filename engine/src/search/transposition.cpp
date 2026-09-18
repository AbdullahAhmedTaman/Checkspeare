#include "transposition.h"
#include <algorithm>

namespace Search
{
    TranspositionTable TT;

    void TranspositionTable::resize(int mb)
    {
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

    void TranspositionTable::store(Key key, int depth, Value score, TTFlag flag, Move move)
    {
        if (table.empty())
            return;

        TTBucket& bucket = table[key & bucket_mask];

        int target_slot = -1;
        if (bucket.entries[0].key == key || bucket.entries[0].key == 0)
            target_slot = 0;
        else if (bucket.entries[1].key == key || bucket.entries[1].key == 0)
            target_slot = 1;
        else if (bucket.entries[2].key == key || bucket.entries[2].key == 0)
            target_slot = 2;
        else if (bucket.entries[3].key == key || bucket.entries[3].key == 0)
            target_slot = 3;

        // Collision Resolution: Two-Tier Replacement Strategy.
        if (target_slot == -1)
        {
            if (depth >= bucket.entries[0].depth)
            {
                // Claim Slot 0: The depth-preferred fortress.
                target_slot = 0;
            }
            else
            {
                // pseudo-random slot selection (1, 2, or 3).
                target_slot = 1 + (key & 1) + ((key >> 1) & 1);
            }
        }

        bucket.entries[target_slot].key = key;
        bucket.entries[target_slot].depth = depth;
        bucket.entries[target_slot].score = score;
        bucket.entries[target_slot].flag = flag;

        // Preserve existing PV moves if the new evaluation fails low (null move).
        if (move.is_ok() && !move.is_null())
        {
            bucket.entries[target_slot].best_move = move;
        }
    }

    bool TranspositionTable::probe(Key key, TTEntry& out_entry) const
    {
        if (table.empty())
            return false;

        const TTBucket& bucket = table[key & bucket_mask];

        if (bucket.entries[0].key == key)
        {
            out_entry = bucket.entries[0];
            return true;
        }
        if (bucket.entries[1].key == key)
        {
            out_entry = bucket.entries[1];
            return true;
        }
        if (bucket.entries[2].key == key)
        {
            out_entry = bucket.entries[2];
            return true;
        }
        if (bucket.entries[3].key == key)
        {
            out_entry = bucket.entries[3];
            return true;
        }

        return false;
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