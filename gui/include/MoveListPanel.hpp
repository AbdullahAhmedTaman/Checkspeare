#pragma once

#include "ChessGame.hpp"
#include "GuiGameController.hpp"

struct ImFont;

namespace Gui
{
    /// Renders the move list and navigation controls.
    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller, ImFont* moveListFont);
} // namespace Gui