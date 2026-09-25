#pragma once

#include "ChessGame.hpp"
#include "GuiGameController.hpp"

namespace Gui
{

    /// Renders the move list and navigation controls.
    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller);

} // namespace Gui