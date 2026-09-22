#pragma once

#include "types.h"

#include <cstdint>
#include <optional>

/// Two-sided chess clock with increment.
///
/// Passive: owns no timer or thread. The owner calls update(dtSeconds) each
/// frame; only the side to move loses time. All durations are in milliseconds
/// to avoid float drift.
class ChessClock
{
public:
    struct TimeControl
    {
        std::int64_t initialMs = 5 * 60 * 1000;
        std::int64_t incrementMs = 0;
    };

    /// Sets both sides to `tc.initialMs` and stores the increment.
    /// Does not start ticking.
    void configure(const TimeControl& tc);

    /// Reapplies the configured TimeControl and clears flag state.
    void reset();

    void start();
    void stop();
    bool running() const;

    /// Advances the active side by `dtSeconds`.
    /// Returns the side whose flag fell this tick, if any.
    std::optional<Color> update(double dtSeconds);

    /// Adds the increment to `mover` and switches the active side.
    void onMovePlayed(Color mover);

    std::int64_t remainingMs(Color side) const;
    bool flagFallen(Color side) const;

private:
    TimeControl tc_{};
    std::int64_t remaining_[2] = {0, 0}; // [0] = WHITE, [1] = BLACK
    Color active_ = WHITE;
    bool running_ = false;
    bool flagged_[2] = {false, false};
};