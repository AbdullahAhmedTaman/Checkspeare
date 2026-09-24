#pragma once

#include "ChessGame.hpp"
#include "GuiGameController.hpp"

namespace Gui
{

    /// Renders the move list (chess.com style: two columns, move number on the
    /// left) and the Undo / Redo buttons below it. Queues actions on the
    /// controller; does not mutate the game directly.
    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller, const BoardGeometry& geo);

} // namespace Gui