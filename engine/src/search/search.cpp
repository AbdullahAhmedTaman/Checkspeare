#include "search.h"
#include "heuristics.h"
#include <cstring>

namespace Search
{
    Move killer_moves[MAX_PLY][2] = {};
    int history_table[MAX_PIECES][MAX_SQUARES] = {};

    void init() // clearing the killer moves and history table before a new search
    {
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(history_table, 0, sizeof(history_table));
    }
} // namespace Search