#pragma once

#include "Board.hpp"
#include "types.h"

#include <string>

/// Standard Algebraic Notation for `m` played in `board`.
/// `board` must be the position *before* the move.
///
/// Takes the Board by value: SAN generation needs both the before-state
/// (piece, capture, disambiguation) and the after-state ("+"/"#"), and the
/// function mutates its own copy rather than the caller's.
///
/// @note Not yet implemented.
std::string toSan(Board board, Move m);