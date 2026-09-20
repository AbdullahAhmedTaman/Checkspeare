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
    /// @note Not yet implemented.
    void configure(const TimeControl& tc);

    /// Reapplies the configured TimeControl and clears flag state.
    /// @note Not yet implemented.
    void reset();

    /// @note Not yet implemented.
    void start();

    /// @note Not yet implemented.
    void stop();

    /// @note Not yet implemented.
    bool running() const;

    /// Advances the active side by `dtSeconds`.
    /// Returns the side whose flag fell this tick, if any.
    /// @note Not yet implemented.
    std::optional<Color> update(double dtSeconds);

    /// Adds the increment to `mover` and switches the active side.
    /// @note Not yet implemented.
    void onMovePlayed(Color mover);

    /// @note Not yet implemented.
    std::int64_t remainingMs(Color side) const;

    /// @note Not yet implemented.
    bool flagFallen(Color side) const;
};