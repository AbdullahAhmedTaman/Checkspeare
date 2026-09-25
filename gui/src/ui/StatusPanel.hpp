#pragma once

#include "../game/ChessGame.hpp"
#include "../controller/GuiGameController.hpp"

namespace Gui
{

    /// Renders the "Game" panel: whose turn it is, the latest controller
    /// feedback, and a Resign button enabled when `canResign` is true.
    void renderStatusPanel(GuiGameController& controller, const ChessGame& game, bool canResign);

} // namespace Gui