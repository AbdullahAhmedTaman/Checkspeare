#pragma once

#include "types.h"

struct Position;

extern Key piece_keys[12][64];
extern Key side_key;
extern Key castle_keys[16];
extern Key en_passant_keys[8];

namespace Zobrist
{
    // Must be called once before any hashing.
    void init();

    Key generate_key(const Position& pos);
} // namespace Zobrist