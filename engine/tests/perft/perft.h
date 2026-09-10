#pragma once

#include "../../include/position.h"
#include <cstdint>

namespace Perft
{

    uint64_t run(Position& pos, int depth);

    void divide(Position& pos, int depth);
} // namespace Perft
