#pragma once

#include "controller/GuiGameController.hpp"
#include "game/ChessGame.hpp"

#include <string>

namespace Gui
{
    /// Renders the FEN field with Apply / Refresh / Copy buttons.
    ///
    /// Apply does not touch the game directly: it queues the FEN on the
    /// controller, and Application drains it in stepSimulation so engines
    /// can be aborted before the position changes. `errorOut` is written
    /// by Application when a load fails for a reason the panel cannot see
    /// (a semantic FEN error, not a syntax error). The panel clears it
    /// when the user hits Apply or Refresh.
    void renderFenPanel(ChessGame& game, GuiGameController& controller, std::string& errorOut);

} // namespace Gui