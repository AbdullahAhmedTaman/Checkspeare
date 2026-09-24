#pragma once

#include "ChessGame.hpp"
#include "GuiGameController.hpp"

namespace Gui
{

    /// Renders the "Game" panel: whose turn it is, the latest controller
    /// feedback, and — when the game has ended — a colored result banner
    /// in place of a separate popup window.
    void renderStatusPanel(const GuiGameController& controller, const ChessGame& game);

} // namespace Gui