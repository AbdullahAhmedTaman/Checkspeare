#include "ChessGame.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    /// Structural checks that Position::from_fen is not required to perform.
    /// Kept deliberately small: we only reject FENs that would make
    /// MoveGen::generate_legal_moves misbehave.
    void validateParsedPosition(const Position& p, const std::string& fen)
    {
        if (p.king_square(WHITE) == SQ_NONE || p.king_square(BLACK) == SQ_NONE)
            throw std::invalid_argument("[ChessGame] FEN is missing a king: " + fen);

        // The side *not* to move must not be in check. Otherwise the side to
        // move could "capture" the king, which MoveGen does not model.
        const Color them = (p.sideToMove == WHITE) ? BLACK : WHITE;
        if (p.is_square_attacked(p.king_square(them), p.sideToMove))
            throw std::invalid_argument("[ChessGame] FEN has side-not-to-move in check: " + fen);
    }
} // namespace

ChessGame::ChessGame()
{
    pos_ = Position::from_fen(START_FEN);
    refreshLegalMoves();
}

void ChessGame::loadFEN(const std::string& fen)
{
    // Parse into a local first so a rejection does not corrupt live state.
    Position candidate = Position::from_fen(fen);
    validateParsedPosition(candidate, fen);

    pos_ = std::move(candidate);
    refreshLegalMoves();
}

void ChessGame::reset()
{
    pos_ = Position::from_fen(START_FEN);
    refreshLegalMoves();
}

void ChessGame::refreshLegalMoves()
{
    // Invariant established by loadFEN / reset / make_move.
    // Kept as asserts because they are cheap and document the contract.
    assert(pos_.king_square(WHITE) != SQ_NONE);
    assert(pos_.king_square(BLACK) != SQ_NONE);

    legalMoves_.count = 0;
    MoveGen::generate_legal_moves(pos_, legalMoves_);
}

Piece ChessGame::pieceAt(Square sq) const
{
    if (!is_valid(sq))
        return PIECE_NONE;
    return pos_.board[sq];
}

Color ChessGame::sideToMove() const
{
    return pos_.sideToMove;
}

bool ChessGame::isCheckmate() const
{
    if (legalMoves_.count != 0)
        return false;
    const Color opponent = (pos_.sideToMove == WHITE) ? BLACK : WHITE;
    return pos_.is_square_attacked(pos_.king_square(pos_.sideToMove), opponent);
}

bool ChessGame::isStalemate() const
{
    if (legalMoves_.count != 0)
        return false;
    const Color opponent = (pos_.sideToMove == WHITE) ? BLACK : WHITE;
    return !pos_.is_square_attacked(pos_.king_square(pos_.sideToMove), opponent);
}

std::vector<Square> ChessGame::legalDestinations(Square fromSq) const
{
    std::vector<Square> dests;
    if (!is_valid(fromSq))
        return dests;

    // Deduplicate: promotions to the same square appear as multiple Moves.
    bool seen[64] = {};
    for (const Move& m : legalMovesFrom(fromSq))
    {
        const Square to = m.to();
        if (!seen[to])
        {
            seen[to] = true;
            dests.push_back(to);
        }
    }
    return dests;
}

std::vector<Move> ChessGame::legalMovesFrom(Square fromSq) const
{
    std::vector<Move> moves;
    if (!is_valid(fromSq))
        return moves;

    for (const Move* m = legalMoves_.begin(); m != legalMoves_.end(); ++m)
    {
        if (m->from() == fromSq)
            moves.push_back(*m);
    }
    return moves;
}

bool ChessGame::tryMove(Square from, Square to, PieceType promoType)
{
    if (!is_valid(from) || !is_valid(to))
        return false;

    for (const Move* m = legalMoves_.begin(); m != legalMoves_.end(); ++m)
    {
        if (m->from() != from || m->to() != to)
            continue;
        if (m->is_promotion() && promotion_type(*m) != promoType)
            continue;

        pos_.make_move(*m);
        refreshLegalMoves();
        return true;
    }
    return false;
}