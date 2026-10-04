#pragma once

#include "../../../engine/src/board/types.h"
#include "../game/ChessClock.hpp"
#include "../game/ChessGame.hpp"
#include "UciEngine.hpp"


#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>


enum class EnginePlayerAction
{
    Idle,
    StartSearch,
    AbortSearch,
    FinishSearch,
};

/// All the inputs the state machine needs to make one decision.
struct EnginePlayerInputs
{
    // Player state.
    bool thinking = false;
    std::size_t thinkStartPly = 0;

    // Game state.
    std::size_t gameCursor = 0;
    std::size_t historySize = 0;
    Color sideToMove = WHITE;
    bool sideControlled = false;

    /// True when the game is over (mate, stalemate, or clock flagged).
    /// The engine will not start a new search while this is set.
    bool gameOver = false;

    // Engine state.
    bool engineReady = false;
    bool engineAlive = false;
    bool bestMoveArrived = false;
};

EnginePlayerAction decideEngineAction(const EnginePlayerInputs& in);

std::optional<Move> parseUciMove(const Board& board, std::string_view text);

class EnginePlayer
{
public:
    explicit EnginePlayer(UciEngine& engine);

    void setSideControlled(Color side, bool controlled);
    bool isSideControlled(Color side) const;

    /// Drives one frame of the state machine. Returns the move played this
    /// tick, if any.
    std::optional<Move> tick(ChessGame& game, ChessClock& clock);

    void abort();

    bool thinking() const
    {
        return thinking_;
    }

    const std::optional<UciInfo>& lastInfo() const
    {
        return lastInfo_;
    }

    Color lastInfoSide() const
    {
        return lastInfoSide_;
    }

private:
    UciEngine& engine_;
    bool controlled_[2] = {false, false};
    bool thinking_ = false;
    std::size_t thinkStartPly_ = 0;
    std::optional<UciInfo> lastInfo_;
    Color lastInfoSide_ = WHITE;
    std::optional<std::string> pendingBestMove_;
    std::chrono::steady_clock::time_point thinkStartTime_{};
};