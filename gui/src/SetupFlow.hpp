#pragma once

#include "ui/dialogs/GameSetupPanel.hpp"

#include <functional>

struct Config;
class ChessGame;
class ChessClock;
class EngineRig;

namespace Gui
{
    class GuiGameController;
    struct BoardGeometry;
} // namespace Gui

/// Owns the setup modal's edit buffer and open/closed state, and applies
/// a committed setup to the game, clock, engines, board orientation, and
/// controller.
///
/// The modal can be opened without any of those being present; it only
/// needs the edit buffer. Application remains responsible for its own
/// per-frame bookkeeping (cursor tracking, game-over latch) via the
/// onApplied callback.
class SetupFlow
{
public:
    SetupFlow(Config& conf, ChessGame& game, ChessClock& clock, EngineRig& engines, Gui::GuiGameController& controller,
              Gui::BoardGeometry& geo);

    /// Seeds the edit buffer from Config. Call once at startup.
    void seedFromConfig();

    /// Renders the setup modal. If the user commits, applies the setup
    /// and invokes the onApplied callback. Returns true on the frame a
    /// setup was committed.
    bool render();

    /// True while the modal is open. Application gates input and skips
    /// engine ticking while this is set.
    bool isOpen() const
    {
        return open_;
    }

    /// Requests the modal be reopened. The current game stays intact
    /// until the user commits a new setup.
    void requestNewGame()
    {
        open_ = true;
    }

    /// Called after a setup is applied, so Application can reset its
    /// per-frame bookkeeping (cursor tracking, game-over latch).
    void setOnApplied(std::function<void()> cb)
    {
        onApplied_ = std::move(cb);
    }

private:
    void apply(const Gui::GameSetup& setup);

    Config& conf_;
    ChessGame& game_;
    ChessClock& clock_;
    EngineRig& engines_;
    Gui::GuiGameController& controller_;
    Gui::BoardGeometry& geo_;

    Gui::GameSetup pendingSetup_;
    bool open_ = true;
    std::function<void()> onApplied_;
};