#pragma once
#include "board/position.h"
#include "board/types.h"

namespace Search
{
    /**
     * @brief Performs an optimized Static Exchange Evaluation (SEE) to determine
     *        if a capture sequence achieves a material delta >= threshold.
     *
     * @param pos Current board position.
     * @param move The capture move to evaluate.
     * @param threshold The target material delta (defaults to 0 for "winning or equal").
     * @return true if the exchange is >= threshold, false otherwise.
     */
    bool see_ge(const Position& pos, Move move, int threshold = 0);
} // namespace Search