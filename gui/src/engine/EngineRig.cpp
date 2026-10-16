#include "engine/EngineRig.hpp"

#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"
#include "ui/engine/EngineThinkingPanel.hpp"

#include <exception>

void EngineRig::setSideControlled(Color side, bool controlled)
{
    if (side == WHITE)
        playerWhite_.setSideControlled(WHITE, controlled);
    else
        playerBlack_.setSideControlled(BLACK, controlled);
}

bool EngineRig::sideIsEngineControlled(Color side) const
{
    // A side is blocked from human input only when an engine is actually
    // running and configured to play it. Configuration alone is a
    // preference; starting an engine is what makes it take effect.
    const bool whiteWouldPlay = playerWhite_.isSideControlled(side) && engineWhite_.isRunning();
    const bool blackWouldPlay = playerBlack_.isSideControlled(side) && engineBlack_.isRunning();
    return whiteWouldPlay || blackWouldPlay;
}

void EngineRig::launchOrStop(Color side, bool wantEngine, const std::string& path)
{
    UciEngine& engine = (side == WHITE) ? engineWhite_ : engineBlack_;

    if (!wantEngine)
    {
        engine.stop();
        return;
    }

    try
    {
        engine.start(path);
    }
    catch (const std::exception& e)
    {
        // Store the message so the popup can show it. The game continues
        // with the engine stopped; the user can fix the path from the
        // Engine Settings panel and try again.
        error_ = e.what();
    }
}

void EngineRig::tick(ChessGame& game, ChessClock& clock)
{
    playerWhite_.tick(game, clock);
    playerBlack_.tick(game, clock);
}

void EngineRig::abortAll()
{
    playerWhite_.abort();
    playerBlack_.abort();
}

void EngineRig::stopAll()
{
    engineWhite_.stop();
    engineBlack_.stop();
}

Gui::EngineThinkingSnapshot EngineRig::buildThinkingSnapshot(const ChessGame& game) const
{
    Gui::EngineThinkingSnapshot snap;

    const bool whiteActive = engineWhite_.isRunning() && playerWhite_.isSideControlled(WHITE);
    const bool blackActive = engineBlack_.isRunning() && playerBlack_.isSideControlled(BLACK);

    const EnginePlayer* active = nullptr;
    const UciEngine* activeEngine = nullptr;

    if (whiteActive && blackActive)
    {
        if (game.sideToMove() == WHITE)
        {
            active = &playerWhite_;
            activeEngine = &engineWhite_;
        }
        else
        {
            active = &playerBlack_;
            activeEngine = &engineBlack_;
        }
    }
    else if (whiteActive)
    {
        active = &playerWhite_;
        activeEngine = &engineWhite_;
    }
    else if (blackActive)
    {
        active = &playerBlack_;
        activeEngine = &engineBlack_;
    }

    if (active == nullptr)
        return snap;

    snap.activeSide = (active == &playerWhite_) ? "White" : "Black";
    snap.engineName = activeEngine->name();
    snap.thinking = active->thinking();

    if (const auto& info = active->lastInfo())
    {
        snap.depth = info->depth;
        snap.seldepth = info->seldepth;
        snap.nodes = info->nodes;
        snap.nps = info->nps;

        // UCI reports from the engine's own point of view; normalize to
        // White's perspective for display.
        const bool fromBlack = (active->lastInfoSide() == BLACK);
        if (info->scoreCp)
            snap.scoreCp = fromBlack ? -(*info->scoreCp) : *info->scoreCp;
        if (info->scoreMate)
            snap.scoreMate = fromBlack ? -(*info->scoreMate) : *info->scoreMate;
    }

    return snap;
}