#pragma once

#include "assets/TextureLibrary.hpp"
#include "game/Board.hpp"


namespace Gui
{

    /// Renders the "Captured" panel: two rows, one per side, showing the
    /// opponent pieces that side has captured so far. The panel reads the
    /// board state directly; it does not consume the move history, so undo
    /// and jump-to-ply are handled for free.
    void renderCapturedPiecesPanel(const Board& board, const Assets::TextureLibrary& assets);

} // namespace Gui