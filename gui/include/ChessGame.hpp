#pragma once

#include "movegen.h"
#include "position.h"
#include "types.h"

#include <string>
#include <vector>

/// Owns a chess Position together with a cache of its legal moves.
/// Exists so GUI code doesn't have to regenerate legal moves on every query.
/// All state is engine-native; no rendering or UI concepts leak in here.
class ChessGame
{
public:
    /// Starts from the standard initial position.
    ChessGame();

    /// Replaces the current position with the one described by `fen`.
    /// Precondition: `fen` must describe a legal position with both kings.
    void loadFEN(const std::string& fen);

    /// Resets to the standard initial position.
    void reset();

    Piece pieceAt(Square sq) const;
    Color sideToMove() const;
    bool isCheckmate() const;
    bool isStalemate() const;

    /// Distinct destination squares reachable from `fromSq`
    /// (promotion moves to the same square are collapsed).
    std::vector<Square> legalDestinations(Square fromSq) const;

    /// Every legal move originating from `fromSq`, including all promotions.
    std::vector<Move> legalMovesFrom(Square fromSq) const;

    /// Attempts to play `from -> to`. Promotion defaults to a queen.
    /// Returns true if a matching legal move was applied.
    bool tryMove(Square from, Square to, PieceType promoType = QUEEN);

    /// Read-only access to the underlying position (e.g. for ImGui panels
    /// that want to display FEN, half-move clock, castling rights, etc.).
    const Position& position() const
    {
        return pos_;
    }

private:
    Position pos_;
    MoveList legalMoves_;

    void refreshLegalMoves();
};