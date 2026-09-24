#include "ChessClock.hpp"

#include <algorithm>
#include <cmath>

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
    // The increment is not granted to a side whose clock has already
    // fallen. Once flagged, that side's time is locked at 0 until reset.
    if (!flagged_[mover] && tc_.incrementMs > 0)
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