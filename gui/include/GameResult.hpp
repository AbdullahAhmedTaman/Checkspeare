#pragma once

#include "types.h"

#include <string>

/// Classification of the current game state.
///
/// Priority order, as checked by ChessGame::result():
///   1. Checkmate              (no legal moves, side to move is in check)
///   2. Stalemate              (no legal moves, side to move is not in check)
///   3. Fifty-move             (halfmove clock >= 100)
///   4. Threefold              (current key seen 3+ times in history)
///   5. Insufficient material
///   6. Ongoing
///
/// Note: flag fall is NOT in this enum. ChessClock owns time, ChessGame
/// owns the position, and neither knows about the other. The combined
/// "is the game over" decision lives in Application, which sees both.
enum class GameResultKind
{
    Ongoing,
    Checkmate,
    Stalemate,
    FiftyMove,
    Threefold,
    InsufficientMaterial,

    /// A player resigned. Asserted externally, unlike the other kinds,
    /// which are derived from the position or the clock. `winner` holds
    /// the side that did NOT resign.
    Resignation,
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

/// Human-readable text for a GameResult, or empty string for Ongoing.
///
/// This is the single source of truth for how results are worded. Every
/// panel that displays a result calls this instead of carrying its own
/// switch statement. The one exception is the flag-fall text, which is
/// assembled by the panel that has access to the clock.
inline std::string toString(const GameResult& r)
{
    switch (r.kind)
    {
        case GameResultKind::Ongoing:
            return "";
        case GameResultKind::Checkmate:
            return (r.winner == WHITE) ? "White wins" : "Black wins";
        case GameResultKind::Stalemate:
            return "Stalemate - Draw";
        case GameResultKind::FiftyMove:
            return "Draw by fifty-move rule";
        case GameResultKind::Threefold:
            return "Draw by threefold repetition";
        case GameResultKind::InsufficientMaterial:
            return "Draw by insufficient material";
        case GameResultKind::Resignation:
            return (r.winner == WHITE) ? "White wins" : "Black wins";
    }
    return "";
}