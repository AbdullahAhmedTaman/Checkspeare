#pragma once

#include "movegen.h"
#include "position.h"
#include "types.h"

#include <optional>
#include <string>
#include <vector>

/// Owns a chess Position together with a cache of its legal moves.
/// Exists so GUI code does not have to regenerate legal moves per query.
/// Engine-native only; no UI concepts.
class Board
{
public:
    Board();

    /// Replaces the current position with the one described by `fen`.
    /// Precondition: `fen` describes a legal position with both kings.
    void loadFEN(const std::string& fen);

    void reset();

    Piece pieceAt(Square sq) const;
    Color sideToMove() const;
    bool isCheckmate() const;
    bool isStalemate() const;
    bool inCheck() const;

    /// Distinct destination squares reachable from `fromSq`. Promotions to
    /// the same square are collapsed into one entry.
    std::vector<Square> legalDestinations(Square fromSq) const;

    /// Every legal move from `fromSq`, including all promotion variants.
    std::vector<Move> legalMovesFrom(Square fromSq) const;

    std::vector<Move> allLegalMoves() const;

    /// Plays `from -> to`. Promotions default to queen.
    /// Returns the applied Move on success, std::nullopt otherwise.
    std::optional<Move> tryMove(Square from, Square to, PieceType promoType = QUEEN);

    /// Low-level forward/back on the underlying Position.
    /// Must be kept in sync with ChessGame's move list; do not call from GUI code.
    void applyMove(Move m);
    void unmakeMove(Move m);

    /// Read-only access to the underlying position for panels that need FEN,
    /// half-move clock, castling rights, etc.
    const Position& position() const
    {
        return pos_;
    }

private:
    Position pos_;
    MoveList legalMoves_;

    void refreshLegalMoves();
};