#pragma once

#include "ChessClock.hpp"
#include "ChessGame.hpp"

// Forward-declared; see ImGuiLayer.hpp.
struct ImFont;

namespace Gui
{

    /// Renders the "Clock" panel: two player cards side by side, each
    /// showing the remaining time in a large readout and a bar whose fill
    /// is the fraction of the leading side's clock. The side to move is
    /// highlighted; a flagged side is drawn in red.
    ///
    /// `clockFont` should be a font rasterized at the size the numbers are
    /// drawn at (36px in the current layout). If null, the panel falls back
    /// to the default UI font scaled up, which is readable but soft.
    void renderClockPanel(const ChessClock& clock, const ChessGame& game, ImFont* clockFont);

} // namespace Gui