#pragma once

#include "Board.hpp"
#include "GameResult.hpp"
#include "MoveRecord.hpp"
#include "types.h"

#include <cstddef>
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
class ChessGame
{
public:
    ChessGame();

    /// Replaces the current game with `fen` and clears history.
    void loadFEN(const std::string& fen);

    /// Resets to the standard initial position and clears history.
    void reset();

    /// Plays from -> to. If the cursor is behind the end of the list (user
    /// navigated back and then played), the redo tail is discarded first.
    /// Returns the applied Move on success.
    std::optional<Move> play(Square from, Square to, PieceType promo = QUEEN);

    /// Steps the cursor one ply back. False if already at ply 0.
    bool undo();

    /// Steps the cursor one ply forward. False if already at the end.
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

    /// Full history, including entries past the cursor (the redo tail).
    const std::vector<MoveRecord>& history() const
    {
        return moves_;
    }

    /// Entries [0, cursor_). What the move-list panel should render.
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
    /// Applies moves_[cursor_] and advances. Precondition: cursor_ < size.
    void stepForward();

    /// Unapplies moves_[cursor_-1] and retreats. Precondition: cursor_ > 0.
    void stepBackward();

    /// Finds the matching legal move in the current position without
    /// mutating anything. Returns nullopt if from -> to is not legal.
    std::optional<Move> findLegalMove(Square from, Square to, PieceType promo) const;

    /// True if the position has no pawns, rooks, or queens, and at most
    /// one minor piece, or two bishops on the same color. Not exhaustive
    /// for every FIDE-insufficient-material case, but covers the common ones.
    bool isInsufficientMaterial() const;

    Board board_;
    std::vector<MoveRecord> moves_;
    std::size_t cursor_ = 0;
};