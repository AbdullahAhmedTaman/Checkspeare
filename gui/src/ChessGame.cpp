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
    // Playing after navigating back discards the redo tail.
    if (cursor_ < moves_.size())
        moves_.resize(cursor_);

    // SAN needs the position before the move; copy the board first.
    Board before = board_;

    auto m = board_.tryMove(from, to, promo);
    if (!m)
        return std::nullopt;

    MoveRecord rec;
    rec.move = *m;
    // the San appliance
    // rec.san = toSan(before, *m);
    rec.positionKeyAfter = board_.position().key;
    moves_.push_back(std::move(rec));

    ++cursor_;
    return m;
}