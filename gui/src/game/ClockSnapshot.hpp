#pragma once

#include "board/types.h"

#include <cstdint>

/// Value-type snapshot of a chess clock's state. Used to restore the clock
/// when the game cursor moves backward or forward.
///
/// Deliberately lives outside both ChessClock.hpp and MoveRecord.hpp so
/// neither has to include the other. ChessGame carries one of these per
/// MoveRecord plus one for the start position; ChessClock converts to and
/// from it.
struct ClockSnapshot
{
    std::int64_t remainingWhiteMs = 0;
    std::int64_t remainingBlackMs = 0;
    Color active = WHITE;
    bool flaggedWhite = false;
    bool flaggedBlack = false;
};