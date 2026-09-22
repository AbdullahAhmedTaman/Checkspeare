#include "ChessClock.hpp"

#include <algorithm> // std::clamp
#include <cmath>     // std::floor

namespace
{
    // milliseconds are stored as int64. dtSeconds comes in as a float/double,
    // so we convert once per call. Using llround to avoid accumulating
    // truncation error over many frames.
    std::int64_t toMs(double seconds)
    {
        return static_cast<std::int64_t>(std::llround(seconds * 1000.0));
    }
} // namespace

void ChessClock::configure(const TimeControl& tc)
{
    tc_ = tc;
    remaining_[0] = tc.initialMs;
    remaining_[1] = tc.initialMs;
    active_ = WHITE;
    running_ = false;
    flagged_[0] = false;
    flagged_[1] = false;
}

void ChessClock::reset()
{
    configure(tc_);
}

void ChessClock::start()
{
    running_ = true;
}

void ChessClock::stop()
{
    running_ = false;
}

bool ChessClock::running() const
{
    return running_;
}

std::optional<Color> ChessClock::update(double dtSeconds)
{
    if (!running_)
        return std::nullopt;
    if (dtSeconds <= 0.0)
        return std::nullopt;

    // If the active side already flagged, do nothing.
    if (flagged_[active_])
        return std::nullopt;

    const std::int64_t before = remaining_[active_];
    const std::int64_t delta = toMs(dtSeconds);
    remaining_[active_] = before - delta;

    if (remaining_[active_] <= 0)
    {
        remaining_[active_] = 0;
        flagged_[active_] = true;
        return active_; // this side's flag just fell
    }
    return std::nullopt;
}

void ChessClock::onMovePlayed(Color mover)
{
    // Add increment to the mover.
    if (tc_.incrementMs > 0)
        remaining_[mover] += tc_.incrementMs;

    // Flip the active side.
    active_ = (mover == WHITE) ? BLACK : WHITE;
}

std::int64_t ChessClock::remainingMs(Color side) const
{
    return remaining_[side];
}

bool ChessClock::flagFallen(Color side) const
{
    return flagged_[side];
}

/// So we're clear on scope:

/// It doesn't auto-declare a loss on flag fall. It returns which side flagged;
// the caller decides what to do. Wiring flag-fall into ChessGame::result()
// is a separate design decision (does the game model a time loss? a draw? a special GameResultKind?).
// I'd defer that to a later block — for the MVP, showing "White's flag fell" in the UI is enough.

/// It doesn't pause on undo/redo. If you undo two moves, the clock keeps its current time.
// This is technically wrong (a real clock would restore the time from before those moves),
// but it's the standard MVP compromise: "undo doesn't rewind the clock.
// " A later iteration could store the time in MoveRecord.

/// It doesn't handle multiple time controls (Fischer, Bronstein, byo-yomi). Just standard increment.