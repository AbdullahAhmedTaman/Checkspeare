#pragma once

#include "ChessGame.hpp"
#include "GuiGameController.hpp"

namespace Gui
{

    /// Renders the "Game" panel: whose turn it is, the latest controller
    /// feedback, and — when the game has ended — a colored result banner
    /// with a New Game button.
    ///
    /// The controller is taken by non-const reference because the New Game
    /// button queues a request through it.
    void renderStatusPanel(GuiGameController& controller, const ChessGame& game);

} // namespace Gui