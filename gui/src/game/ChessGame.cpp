#include "ChessGame.hpp"
#include "../audio/MoveSound.hpp"
#include "GameResult.hpp"
#include "San.hpp"


namespace
{
    constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    /// Categorises a committed move for sound playback. Priority is
    /// specific-to-general: promotion and castling are distinctive, captures
    /// are more impactful than quiet moves, and check is the fallback
    /// "something happened" category.
    /// Categorises a committed move for sound playback. Priority is
    /// specific-to-general: promotion and castling are distinctive, captures
    /// are more impactful than quiet moves, and check is the fallback
    /// "something happened" category.
    MoveSound classifyMoveSound(const Board& after, Move m)
    {
        if (after.inCheck())
            return MoveSound::Check;
        if (m.is_promotion())
            return MoveSound::Promotion;
        if (m.is_castle())
            return MoveSound::Castle;
        if (m.is_capture())
            return MoveSound::Capture;
        if (m.is_ep())
            return MoveSound::Capture;
        return MoveSound::QuietMove;
    }
} // namespace

ChessGame::ChessGame()
{
    board_.loadFEN(START_FEN);
}

void ChessGame::loadFEN(const std::string& fen)
{
    board_.loadFEN(fen);
    moves_.clear();
    cursor_ = 0;
    resigned_.reset();
    syncClockToCursor();
}

void ChessGame::reset()
{
    board_.reset();
    moves_.clear();
    cursor_ = 0;
    resigned_.reset();
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
    // immediately so the clock reflects it this frame.
    if (cursor_ == ply + 1)
        syncClockToCursor();
}

void ChessGame::setMoveCommitted(MoveCommitted cb)
{
    moveCommitted_ = std::move(cb);
}

std::optional<Move> ChessGame::play(Square from, Square to, PieceType promo)
{
    const auto m = findLegalMove(from, to, promo);
    if (!m)
        return std::nullopt;

    if (cursor_ < moves_.size())
        moves_.resize(cursor_);

    const Color mover = board_.sideToMove();

    Board before = board_;
    board_.applyMove(*m);
    ++cursor_;

    MoveRecord rec;
    rec.move = *m;
    rec.san = toSan(before, *m);
    rec.sound = classifyMoveSound(board_, *m);
    rec.positionKeyAfter = board_.position().key;
    // rec.clockAfter is set by the moveCommitted_ callback below.
    moves_.push_back(std::move(rec));

    // A genuine commit: the only path that may touch the clock's increment
    // and active side. Navigation only ever restores snapshots.
    if (moveCommitted_)
        moveCommitted_(cursor_ - 1, mover);

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

void ChessGame::resign(Color side)
{
    resigned_ = side;
}

std::optional<Color> ChessGame::resignation() const
{
    return resigned_;
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
    gotoPly(moves_.size());
}

void ChessGame::gotoStart()
{
    gotoPly(0);
}

std::vector<MoveRecord> ChessGame::historyToCursor() const
{
    return {moves_.begin(), moves_.begin() + static_cast<std::ptrdiff_t>(cursor_)};
}

GameResult ChessGame::result() const
{
    // Resignation takes priority: it is an asserted decision to end the
    // game, and no position-derived classification can override it.
    if (resigned_.has_value())
    {
        const Color winner = (*resigned_ == WHITE) ? BLACK : WHITE;
        return {GameResultKind::Resignation, winner};
    }

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