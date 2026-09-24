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
    syncClockToCursor();
}

void ChessGame::reset()
{
    board_.reset();
    moves_.clear();
    cursor_ = 0;
    syncClockToCursor();
}

void ChessGame::setClockSync(ClockSync sync)
{
    clockSync_ = std::move(sync);
}

void ChessGame::setInitialClockSnapshot(const ClockSnapshot& s)
{
    initialClock_ = s;
    if (cursor_ == 0)
        syncClockToCursor();
}

void ChessGame::setClockSnapshotAt(std::size_t ply, const ClockSnapshot& s)
{
    if (ply >= moves_.size())
        return;

    moves_[ply].clockAfter = s;

    // If the cursor is exactly on this ply, push the new snapshot through
    // as well, so the clock reflects it immediately.
    if (cursor_ == ply + 1)
        syncClockToCursor();
}

std::optional<Move> ChessGame::play(Square from, Square to, PieceType promo)
{
    const auto m = findLegalMove(from, to, promo);
    if (!m)
        return std::nullopt;

    if (cursor_ < moves_.size())
        moves_.resize(cursor_);

    Board before = board_;
    board_.applyMove(*m);
    ++cursor_;

    MoveRecord rec;
    rec.move = *m;
    rec.san = toSan(before, *m);
    rec.positionKeyAfter = board_.position().key;
    // rec.clockAfter is set later by Application via setClockSnapshotAt.
    moves_.push_back(std::move(rec));

    return m;
}

bool ChessGame::undo()
{
    if (cursor_ == 0)
        return false;
    stepBackward();
    syncClockToCursor();
    return true;
}

bool ChessGame::redo()
{
    if (cursor_ == moves_.size())
        return false;
    stepForward();
    syncClockToCursor();
    return true;
}

void ChessGame::gotoPly(std::size_t ply)
{
    if (ply > moves_.size())
        ply = moves_.size();

    bool changed = false;
    while (cursor_ > ply)
    {
        stepBackward();
        changed = true;
    }
    while (cursor_ < ply)
    {
        stepForward();
        changed = true;
    }
    if (changed)
        syncClockToCursor();
}

void ChessGame::gotoEnd()
{
    bool changed = false;
    while (cursor_ < moves_.size())
    {
        stepForward();
        changed = true;
    }
    if (changed)
        syncClockToCursor();
}

void ChessGame::gotoStart()
{
    bool changed = false;
    while (cursor_ > 0)
    {
        stepBackward();
        changed = true;
    }
    if (changed)
        syncClockToCursor();
}

std::vector<MoveRecord> ChessGame::historyToCursor() const
{
    return {moves_.begin(), moves_.begin() + static_cast<std::ptrdiff_t>(cursor_)};
}

GameResult ChessGame::result() const
{
    if (board_.isCheckmate())
    {
        const Color loser = board_.sideToMove();
        const Color winner = (loser == WHITE) ? BLACK : WHITE;
        return {GameResultKind::Checkmate, winner};
    }
    if (board_.isStalemate())
        return {GameResultKind::Stalemate};

    if (board_.position().halfmoveClock >= 100)
        return {GameResultKind::FiftyMove};

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

    if (isInsufficientMaterial())
        return {GameResultKind::InsufficientMaterial};

    return {};
}

void ChessGame::stepForward()
{
    board_.applyMove(moves_[cursor_].move);
    ++cursor_;
}

void ChessGame::stepBackward()
{
    --cursor_;
    board_.unmakeMove(moves_[cursor_].move);
}

void ChessGame::syncClockToCursor()
{
    if (!clockSync_)
        return;

    if (cursor_ == 0)
        clockSync_(initialClock_);
    else
        clockSync_(moves_[cursor_ - 1].clockAfter);
}

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
        return true;
    if (minorCount == 1)
        return true;
    if (minorCount == 2 && bishopSeen && !knightSeen && !oppositeColorBishops)
        return true;

    return false;
}