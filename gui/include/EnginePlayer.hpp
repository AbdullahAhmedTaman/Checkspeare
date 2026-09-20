#pragma once

#include "ChessClock.hpp"
#include "ChessGame.hpp"
#include "UciEngine.hpp"
#include "types.h"

#include <optional>

/// Non-blocking glue between the UCI engine, the game, and the clock.
///
/// tick() is called once per frame. It detects whose turn it is, sends
/// `position` + `go` to the engine on turn change, polls for `bestmove`,
/// and plays the returned move. Never blocks; never touches SFML or ImGui;
/// never owns the engine or the game.
class EnginePlayer
{
public:
    /// @note Not yet implemented.
    explicit EnginePlayer(UciEngine& engine);

    /// Enables or disables engine control for a side.
    /// Both sides may be engine-controlled.
    /// @note Not yet implemented.
    void setSideControlled(Color side, bool controlled);

    /// @note Not yet implemented.
    bool isSideControlled(Color side) const;

    /// Drives one frame of the state machine. Returns the move played this
    /// tick, if any.
    /// @note Not yet implemented.
    std::optional<Move> tick(ChessGame& game, ChessClock& clock);

    /// Abandons any in-flight search. Call after undo or loadFEN.
    /// @note Not yet implemented.
    void abort();

private:
    UciEngine& engine_;
    bool controlled_[2] = {false, false};
    bool thinking_ = false;
};