#pragma once

#include "types.h"

/// Classification of the current game state.
///
/// Priority order, as checked by ChessGame::result():
///   1. Checkmate              (no legal moves, side to move is in check)
///   2. Stalemate              (no legal moves, side to move is not in check)
///   3. Fifty-move             (halfmove clock >= 100)
///   4. Threefold              (current key seen 3+ times in history)
///   5. Insufficient material
///   6. Ongoing
enum class GameResultKind
{
    Ongoing,
    Checkmate,
    Stalemate,
    FiftyMove,
    Threefold,
    InsufficientMaterial
};

/// Value returned by ChessGame::result(). Pure data; no behaviour.
///
/// `winner` is meaningful only when kind == Checkmate. It holds the side
/// that delivered mate (i.e. the side NOT to move). For every draw kind,
/// `winner` is ignored.
struct GameResult
{
    GameResultKind kind = GameResultKind::Ongoing;
    Color winner = WHITE;
};