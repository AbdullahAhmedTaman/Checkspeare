#pragma once

#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"
#include "controller/GuiGameController.hpp"

namespace Gui
{

    /// Renders the game-over modal when the game is terminal and the user
    /// has not dismissed it. Offers two actions: New Game (resets game +
    /// clock) and Close (dismisses the modal for the rest of this game).
    ///
    /// The modal is opened by ImGuiLayer::renderImGui after all other
    /// panels, so it draws on top.
    ///
    /// The modal also treats a clock flag-fall as terminal. It reads the
    /// clock through `flagFallen` on the passed clock; callers that don't
    /// have a clock pass a default-constructed one, in which no side is
    /// flagged and the modal falls back to game.result() only.
    void renderGameOverModal(GuiGameController& controller, const ChessGame& game, const ChessClock& clock, bool setupModalOpen);

} // namespace Gui