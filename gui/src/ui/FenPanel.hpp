#pragma once

#include "../controller/GuiGameController.hpp"
#include "../game/ChessGame.hpp"

namespace Gui
{

    /// Renders the "FEN" panel: a text field prefilled with the current
    /// position's FEN, an Apply button that loads the pasted FEN, a
    /// Refresh button that reseeds the field from the current game, and a
    /// Copy button that puts the field's contents on the clipboard.
    ///
    /// Applying a FEN wipes the move history and resets the cursor. The
    /// panel also clears the controller's analysis arrows and selection,
    /// since both are anchored to squares that may not exist in the new
    /// position.
    void renderFenPanel(ChessGame& game, GuiGameController& controller);

} // namespace Gui