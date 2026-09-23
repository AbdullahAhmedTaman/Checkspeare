#pragma once

#include "ChessClock.hpp"
#include "ChessGame.hpp"
#include "UciEngine.hpp"
#include "types.h"

#include <cstddef>
#include <optional>
#include <string_view>

/// What the engine should do on this tick.
enum class EnginePlayerAction
{
    Idle,         // nothing to do; stay in the current state
    StartSearch,  // send position + go; transition to Thinking
    AbortSearch,  // send stop; transition to Idle
    FinishSearch, // a bestmove is available; play it and go Idle
};

/// All the inputs the state machine needs to make one decision.
/// Populated by EnginePlayer::tick from the game, clock, and engine.
struct EnginePlayerInputs
{
    // Player state.
    bool thinking = false;
    std::size_t thinkStartPly = 0;

    // Game state.
    std::size_t gameCursor = 0;
    Color sideToMove = WHITE;
    bool sideControlled = false;

    // Engine state.
    bool engineReady = false;
    bool engineAlive = false;
    bool bestMoveArrived = false;
};

/// Pure decision function. No side effects, no I/O.
EnginePlayerAction decideEngineAction(const EnginePlayerInputs& in);

/// Parses a UCI move string ("e2e4", "e7e8q") into a legal Move in `board`'s
/// current position. Returns nullopt if the string is malformed or matches no
/// legal move.
std::optional<Move> parseUciMove(const Board& board, std::string_view text);

/// Non-blocking glue between the UCI engine, the game, and the clock.
///
/// tick() is called once per frame. It detects whose turn it is, sends
/// `position` + `go` to the engine on turn change, polls for `bestmove`,
/// and plays the returned move. Never blocks; never touches SFML or ImGui;
/// never owns the engine or the game.
class EnginePlayer
{
public:
    explicit EnginePlayer(UciEngine& engine);

    /// Enables or disables engine control for a side.
    /// Both sides may be engine-controlled.
    void setSideControlled(Color side, bool controlled);

    bool isSideControlled(Color side) const;

    /// Drives one frame of the state machine. Returns the move played this
    /// tick, if any.
    std::optional<Move> tick(ChessGame& game, ChessClock& clock);

    /// Abandons any in-flight search. Call after undo or loadFEN.
    void abort();

private:
    UciEngine& engine_;
    bool controlled_[2] = {false, false};
    bool thinking_ = false;
    std::size_t thinkStartPly_ = 0;
};