#pragma once

#include "engine/EnginePlayer.hpp"
#include "engine/UciEngine.hpp"

#include <string>

class ChessClock;
class ChessGame;

namespace Gui
{
    struct EngineThinkingSnapshot;
} // namespace Gui

/// Owns both engines, their per-side EnginePlayer state machines, and
/// the last engine-start error. Encapsulates "which side is played by
/// which engine" and the per-frame lifecycle (tick, abort, launch/stop).
///
/// Not copyable or movable: each EnginePlayer holds a reference to its
/// UciEngine, so both must live in the same object for their lifetime.
class EngineRig
{
public:
    EngineRig() = default;

    EngineRig(const EngineRig&) = delete;
    EngineRig& operator=(const EngineRig&) = delete;

    /// Records whether `side` is engine-controlled. Does not start or
    /// stop the engine; use launchOrStop for that.
    void setSideControlled(Color side, bool controlled);

    /// True when an engine is running and configured to play `side`.
    bool sideIsEngineControlled(Color side) const;

    /// Starts the engine for `side` when `wantEngine` is true, stops it
    /// otherwise. Start failures are captured in error() instead of thrown.
    void launchOrStop(Color side, bool wantEngine, const std::string& path);

    /// Advances both engines by one frame.
    void tick(ChessGame& game, ChessClock& clock);

    /// Aborts both engines' in-flight searches. Call before any cursor
    /// jump or position swap that would invalidate a search.
    void abortAll();

    /// Stops both engine processes.
    void stopAll();

    /// Snapshot of the currently-searching engine for the thinking panel.
    /// Default-constructed when neither side is being played by a running
    /// engine.
    Gui::EngineThinkingSnapshot buildThinkingSnapshot(const ChessGame& game) const;

    /// Read-only engine access, for display (name, running state).
    const UciEngine& whiteEngine() const
    {
        return engineWhite_;
    }

    const UciEngine& blackEngine() const
    {
        return engineBlack_;
    }

    /// Last engine start failure. Empty if none.
    std::string& error()
    {
        return error_;
    }

private:
    UciEngine engineWhite_;
    UciEngine engineBlack_;

    // Declaration order matters: each player holds a reference to its
    // engine, so the engines must be constructed first.
    EnginePlayer playerWhite_{engineWhite_};
    EnginePlayer playerBlack_{engineBlack_};

    std::string error_;
};