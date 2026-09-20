#pragma once

#include "types.h"

#include <string>

/// One entry in the move history. Produced once at commit time and never
/// mutated afterwards.
///
/// Stores the raw Move plus data that would otherwise require replaying the
/// game per consumer:
///   - SAN text (move list, PGN export, engine PV rendering)
///   - Zobrist key of the position *after* the move (threefold detection)
///
/// No FEN strings: the live position is reproduced from the move list via
/// Position::make_move / unmake_move, so a per-move FEN would be redundant.
struct MoveRecord
{
    Move move = Move::none();
    std::string san;
    Key positionKeyAfter = 0;
};