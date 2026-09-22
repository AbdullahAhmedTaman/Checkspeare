#include "ChessGame.hpp"
#include "GameResult.hpp"
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

GameResult ChessGame::result() const
{
    // 1. Checkmate / stalemate. Board caches the legal move list, so these
    //    are cheap.
    if (board_.isCheckmate())
    {
        const Color loser = board_.sideToMove();
        const Color winner = (loser == WHITE) ? BLACK : WHITE;
        return {GameResultKind::Checkmate, winner};
    }
    if (board_.isStalemate())
        return {GameResultKind::Stalemate};

    // 2. Fifty-move rule. The engine's halfmove field counts plies since
    //    the last capture or pawn move. 100 plies = 50 full moves.
    if (board_.position().halfmoveClock >= 100)
        return {GameResultKind::FiftyMove};

    // 3. Threefold repetition. Walk the history up to the cursor and
    //    count how many times the current position's key appeared.
    const Key currentKey = board_.position().key;
    int occurrences = 0;
    for (std::size_t i = 0; i < cursor_; ++i)
    {
        if (moves_[i].positionKeyAfter == currentKey)
        {
            ++occurrences;
            if (occurrences >= 3)
                return {GameResultKind::Threefold};
        }
    }

    // 4. Insufficient material.
    if (isInsufficientMaterial())
        return {GameResultKind::InsufficientMaterial};

    return {};
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

bool ChessGame::isInsufficientMaterial() const
{
    int minorCount = 0;
    bool knightSeen = false;
    bool bishopSeen = false;
    bool firstBishopIsLight = false;
    bool firstBishopSet = false;
    bool oppositeColorBishops = false;

    for (int i = 0; i < 64; ++i)
    {
        const Square sq = static_cast<Square>(i);
        const Piece pc = board_.pieceAt(sq);
        if (pc == PIECE_NONE)
            continue;

        const PieceType t = type_of(pc);

        // Any of these means material is sufficient.
        if (t == PAWN || t == ROOK || t == QUEEN)
            return false;

        if (t == KNIGHT)
        {
            ++minorCount;
            knightSeen = true;
        }
        else if (t == BISHOP)
        {
            ++minorCount;
            bishopSeen = true;

            const int file = i % 8;
            const int rank = i / 8;
            const bool isLight = ((file + rank) % 2) != 0;

            if (!firstBishopSet)
            {
                firstBishopIsLight = isLight;
                firstBishopSet = true;
            }
            else if (firstBishopIsLight != isLight)
            {
                oppositeColorBishops = true;
            }
        }
    }

    if (minorCount == 0)
        return true; // K vs K
    if (minorCount == 1)
        return true; // K+minor vs K
    if (minorCount == 2 && bishopSeen && !knightSeen && !oppositeColorBishops)
        return true; // K+B vs K+B, same color

    return false;
}