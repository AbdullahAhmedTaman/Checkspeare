#pragma once

#include "../controller/GuiGameController.hpp"
#include "../game/ChessGame.hpp"

namespace Gui
{

    /// Renders the "Game" panel: whose turn it is, the latest controller
    /// feedback, the captured pieces of each side, and a Resign button
    /// enabled when `canResign` is true.
    void renderStatusPanel(GuiGameController& controller, const ChessGame& game, bool canResign);

} // namespace Gui