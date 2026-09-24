#pragma once

#include "Board.hpp"
#include "ClockSnapshot.hpp"
#include "GameResult.hpp"
#include "MoveRecord.hpp"
#include "types.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

/// A chess game: a Board plus the sequence of moves that produced it.
///
/// Undo/redo/seek use Position::make_move and unmake_move via
/// Board::applyMove / Board::unmakeMove. No FEN snapshots are stored; the
/// live position is always the result of walking `moves_` up to `cursor_`.
///
/// Invariant:
///     cursor_ == 0  -> board_ equals the start position
///     cursor_ == n  -> board_ equals start position + moves_[0..n-1]
///
/// The game also carries a clock snapshot per ply so that undo/redo can
/// restore the clock exactly. The game never owns the clock; Application
/// installs a ClockSync callback and pushes snapshots in. Every cursor
/// change invokes the callback with the snapshot that applies to the
/// new cursor position.
class ChessGame
{
public:
    /// Callback invoked with the clock snapshot that applies to the
    /// current cursor after any navigation. Application installs a lambda
    /// that forwards to ChessClock::restore.
    using ClockSync = std::function<void(const ClockSnapshot&)>;

    ChessGame();

    /// Replaces the current game with `fen` and clears history.
    void loadFEN(const std::string& fen);

    /// Resets to the standard initial position and clears history.
    void reset();

    /// Installs the callback that receives clock snapshots on every cursor
    /// change. Passing an empty function disables sync.
    void setClockSync(ClockSync sync);

    /// Records the clock snapshot that applies to the start position.
    /// Called by Application after configuring the clock.
    void setInitialClockSnapshot(const ClockSnapshot& s);

    /// Records the clock snapshot that applies to the move at `ply`.
    /// Called by Application the frame a move is committed.
    void setClockSnapshotAt(std::size_t ply, const ClockSnapshot& s);

    /// Plays from -> to. If the cursor is behind the end of the list (user
    /// navigated back and then played), the redo tail is discarded first.
    /// Returns the applied Move on success.
    std::optional<Move> play(Square from, Square to, PieceType promo = QUEEN);

    bool undo();
    bool redo();

    /// Moves the cursor to `ply` (0 = start position), clamped to
    /// [0, totalMoves()].
    void gotoPly(std::size_t ply);

    void gotoEnd();
    void gotoStart();

    std::size_t totalMoves() const
    {
        return moves_.size();
    }

    std::size_t cursor() const
    {
        return cursor_;
    }

    const std::vector<MoveRecord>& history() const
    {
        return moves_;
    }

    std::vector<MoveRecord> historyToCursor() const;

    /// Classifies the current position. Never mutates.
    GameResult result() const;

    Board& board()
    {
        return board_;
    }

    const Board& board() const
    {
        return board_;
    }

    Piece pieceAt(Square sq) const
    {
        return board_.pieceAt(sq);
    }

    Color sideToMove() const
    {
        return board_.sideToMove();
    }

private:
    void stepForward();
    void stepBackward();

    /// Pushes the snapshot for the current cursor to the sync callback.
    /// Called by every public navigation method.
    void syncClockToCursor();

    std::optional<Move> findLegalMove(Square from, Square to, PieceType promo) const;
    bool isInsufficientMaterial() const;

    Board board_;
    std::vector<MoveRecord> moves_;
    std::size_t cursor_ = 0;

    ClockSnapshot initialClock_;
    ClockSync clockSync_;
};