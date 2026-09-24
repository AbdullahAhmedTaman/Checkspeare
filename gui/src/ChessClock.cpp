#include "ChessClock.hpp"

#include <algorithm>
#include <cmath>

namespace
{
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

    if (flagged_[active_])
        return std::nullopt;

    const std::int64_t before = remaining_[active_];
    const std::int64_t delta = toMs(dtSeconds);
    remaining_[active_] = before - delta;

    if (remaining_[active_] <= 0)
    {
        remaining_[active_] = 0;
        flagged_[active_] = true;
        return active_;
    }
    return std::nullopt;
}

void ChessClock::onMovePlayed(Color mover)
{
    if (!flagged_[mover] && tc_.incrementMs > 0)
        remaining_[mover] += tc_.incrementMs;

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

ClockSnapshot ChessClock::snapshot() const
{
    ClockSnapshot s;
    s.remainingWhiteMs = remaining_[WHITE];
    s.remainingBlackMs = remaining_[BLACK];
    s.active = active_;
    s.flaggedWhite = flagged_[WHITE];
    s.flaggedBlack = flagged_[BLACK];
    return s;
}

void ChessClock::restore(const ClockSnapshot& s)
{
    remaining_[WHITE] = s.remainingWhiteMs;
    remaining_[BLACK] = s.remainingBlackMs;
    active_ = s.active;
    flagged_[WHITE] = s.flaggedWhite;
    flagged_[BLACK] = s.flaggedBlack;
    // running_ is left alone: it reflects whether the game is live, not
    // the specific ply. Undo/redo does not pause or resume the clock.
}