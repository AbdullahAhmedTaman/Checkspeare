#include "../../include/transposition.h"
#include <algorithm>

namespace Search
{
    // Global TT instance allocation
    TranspositionTable TT;

    void TranspositionTable::resize(int mb)
    {
        // Calculate the maximum number of TTEntry structs that fit in the requested MB
        uint64_t bytes = static_cast<uint64_t>(mb) * 1024 * 1024;
        uint64_t num_entries = bytes / sizeof(TTEntry);

        table.resize(num_entries);
        clear();
    }

    void TranspositionTable::clear()
    {
        // Reset all entries to zero/default state
        std::fill(table.begin(), table.end(), TTEntry{});
        entry_count = 0;
    }

    void TranspositionTable::store(Key key, int depth, Value score, TTFlag flag, Move move)
    {
        if (table.empty())
            return;

        // DRAFT MAPPING: Modulo arithmetic (to be replaced by bitwise AND optimization later)
        uint64_t index = key % table.size();
        TTEntry& entry = table[index];

        // DRAFT REPLACEMENT: Depth-Preferred
        // Overwrite if the slot is empty, belongs to a different position, or the new search is deeper
        if (entry.key != key || depth >= entry.depth)
        {
            if (entry.key == 0)
            {
                // Track new unique insertions for hashfull telemetry
                entry_count++;
            }

            entry.key = key;
            entry.depth = depth;
            entry.score = score;
            entry.flag = flag;

            // Preserve the old best move if the new move is null (e.g., from a fail-low node)
            if (move.is_ok() && !move.is_null())
            {
                entry.best_move = move;
            }
        }
    }

    bool TranspositionTable::probe(Key key, TTEntry& out_entry) const
    {
        if (table.empty())
            return false;

        // Map the key to the array index
        uint64_t index = key % table.size();
        const TTEntry& entry = table[index];

        // Verify the stored key perfectly matches to prevent hash collisions
        if (entry.key == key)
        {
            out_entry = entry;
            return true;
        }

        return false;
    }

    int TranspositionTable::hashfull() const
    {
        if (table.empty())
            return 0;

        // Return fullness in per-mill (0 to 1000) standard required by UCI
        return static_cast<int>((entry_count * 1000) / table.size());
    }

} // namespace Search