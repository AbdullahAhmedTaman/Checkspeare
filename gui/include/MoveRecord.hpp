#pragma once

#include "ClockSnapshot.hpp"
#include "MoveSound.hpp"
#include "types.h"

#include <string>

/// One entry in the move history. Produced once at commit time and never
/// mutated except for the clock snapshot, which is filled in by Application
/// on the same frame the move was committed.
///
/// Stores:
///   - the raw Move
///   - SAN text (move list, PGN export, engine PV rendering)
///   - Zobrist key of the position after the move (threefold detection)
///   - the clock's state after the move was played
struct MoveRecord
{
    Move move = Move::none();
    std::string san;
    Key positionKeyAfter = 0;
    ClockSnapshot clockAfter;
    MoveSound sound = MoveSound::QuietMove;
};