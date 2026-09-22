#include "ChessGame.hpp"
#include "San.hpp"

namespace
{
    constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

ChessGame::ChessGame()
{
    board_.loadFEN(START_FEN);
}

void ChessGame::loadFEN(const std::string& fen)
{
    board_.loadFEN(fen);
    moves_.clear();
    cursor_ = 0;
}

void ChessGame::reset()
{
    board_.reset();
    moves_.clear();
    cursor_ = 0;
}

std::optional<Move> ChessGame::play(Square from, Square to, PieceType promo)
{
    // 1. Find the move WITHOUT mutating anything. If it's illegal, we must
    //    not destroy the redo tail.
    const auto m = findLegalMove(from, to, promo);
    if (!m)
        return std::nullopt;

    // 2. Only now is it safe to discard the redo tail.
    if (cursor_ < moves_.size())
        moves_.resize(cursor_);

    // 3. SAN needs the position before the move; copy the board first.
    Board before = board_;

    // 4. Apply.
    board_.applyMove(*m);
    ++cursor_;

    // 5. Build and append the record.
    MoveRecord rec;
    rec.move = *m;
    rec.san = toSan(before, *m);
    rec.positionKeyAfter = board_.position().key;
    moves_.push_back(std::move(rec));

    return m;
}

bool ChessGame::undo()
{
    if (cursor_ == 0)
        return false;
    stepBackward();
    return true;
}

bool ChessGame::redo()
{
    if (cursor_ == moves_.size())
        return false;
    stepForward();
    return true;
}

void ChessGame::gotoPly(std::size_t ply)
{
    if (ply > moves_.size())
        ply = moves_.size();

    while (cursor_ > ply)
        stepBackward();
    while (cursor_ < ply)
        stepForward();
}

void ChessGame::gotoEnd()
{
    while (cursor_ < moves_.size())
        stepForward();
}

void ChessGame::gotoStart()
{
    while (cursor_ > 0)
        stepBackward();
}

std::vector<MoveRecord> ChessGame::historyToCursor() const
{
    return {moves_.begin(), moves_.begin() + static_cast<std::ptrdiff_t>(cursor_)};
}

void ChessGame::stepForward()
{
    // Precondition: cursor_ < moves_.size()
    board_.applyMove(moves_[cursor_].move);
    ++cursor_;
}

void ChessGame::stepBackward()
{
    // Precondition: cursor_ > 0
    --cursor_;
    board_.unmakeMove(moves_[cursor_].move);
}

/// it uses board_.legalMovesFrom(from) rather than board_.allLegalMoves(),
// which is slightly faster (only scans moves from that
/// square). Both work.
std::optional<Move> ChessGame::findLegalMove(Square from, Square to, PieceType promo) const
{
    if (!is_valid(from) || !is_valid(to))
        return std::nullopt;

    for (const Move& m : board_.legalMovesFrom(from))
    {
        if (m.to() != to)
            continue;
        if (m.is_promotion() && promotion_type(m) != promo)
            continue;
        return m;
    }
    return std::nullopt;
}