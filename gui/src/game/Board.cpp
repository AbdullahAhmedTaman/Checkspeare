#include "Board.hpp"

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    /// Structural checks that Position::from_fen does not perform. Kept
    /// minimal: only rejects FENs that would make
    /// MoveGen::generate_legal_moves misbehave.
    void validateParsedPosition(const Position& p, const std::string& fen)
    {
        const Bitboard whiteKings = p.pieces[WHITE][KING];
        const Bitboard blackKings = p.pieces[BLACK][KING];

        if (whiteKings == 0 || blackKings == 0)
            throw std::invalid_argument("[Board] FEN is missing a king: " + fen);

        // The side not to move must not be in check, otherwise the side to
        // move could capture the king, which MoveGen does not model.
        const Color them = (p.sideToMove == WHITE) ? BLACK : WHITE;
        if (p.is_square_attacked(p.king_square(them), p.sideToMove))
            throw std::invalid_argument("[Board] FEN has side-not-to-move in check: " + fen);
    }
} // namespace

Board::Board()
{
    pos_ = Position::from_fen(START_FEN);
    refreshLegalMoves();
}

void Board::loadFEN(const std::string& fen)
{
    // Parse and validate without modifying the live position.
    Position candidate = Position::from_fen(fen);
    validateParsedPosition(candidate, fen);

    // Generate the new legal moves before committing the position.
    MoveList candidateMoves{};
    MoveGen::generate_legal_moves(candidate, candidateMoves);

    // Commit the new state.
    pos_ = std::move(candidate);
    legalMoves_ = candidateMoves;
}

void Board::reset()
{
    pos_ = Position::from_fen(START_FEN);
    refreshLegalMoves();
}

void Board::refreshLegalMoves()
{
    assert(pos_.king_square(WHITE) != SQ_NONE);
    assert(pos_.king_square(BLACK) != SQ_NONE);

    legalMoves_.count = 0;
    MoveGen::generate_legal_moves(pos_, legalMoves_);
}

Piece Board::pieceAt(Square sq) const
{
    if (!is_valid(sq))
        return PIECE_NONE;
    return pos_.board[sq];
}

Color Board::sideToMove() const
{
    return pos_.sideToMove;
}

bool Board::isCheckmate() const
{
    if (legalMoves_.count != 0)
        return false;
    const Color opponent = (pos_.sideToMove == WHITE) ? BLACK : WHITE;
    return pos_.is_square_attacked(pos_.king_square(pos_.sideToMove), opponent);
}

bool Board::isStalemate() const
{
    if (legalMoves_.count != 0)
        return false;
    const Color opponent = (pos_.sideToMove == WHITE) ? BLACK : WHITE;
    return !pos_.is_square_attacked(pos_.king_square(pos_.sideToMove), opponent);
}

bool Board::inCheck() const
{
    const Color them = (pos_.sideToMove == WHITE) ? BLACK : WHITE;
    return pos_.is_square_attacked(pos_.king_square(pos_.sideToMove), them);
}

std::vector<Square> Board::legalDestinations(Square fromSq) const
{
    std::vector<Square> dests;
    if (!is_valid(fromSq))
        return dests;

    // Promotions to the same square appear as multiple Moves; deduplicate.
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

std::vector<Move> Board::legalMovesFrom(Square fromSq) const
{
    std::vector<Move> moves;
    if (!is_valid(fromSq))
        return moves;

    for (const Move& m : legalMoves_)
        if (m.from() == fromSq)
            moves.push_back(m);
    return moves;
}

std::vector<Move> Board::allLegalMoves() const
{
    return {legalMoves_.begin(), legalMoves_.end()};
}

std::optional<Move> Board::tryMove(Square from, Square to, PieceType promoType)
{
    if (!is_valid(from) || !is_valid(to))
        return std::nullopt;

    for (const Move& m : legalMoves_)
    {
        if (m.from() != from || m.to() != to)
            continue;
        if (m.is_promotion() && promotion_type(m) != promoType)
            continue;

        applyMove(m);
        return m;
    }
    return std::nullopt;
}

void Board::applyMove(Move m)
{
    pos_.make_move(m);
    refreshLegalMoves();
}

void Board::unmakeMove(Move m)
{
    pos_.unmake_move(m);
    refreshLegalMoves();
}