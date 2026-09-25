#pragma once

#include "../game/ChessClock.hpp"
#include "../game/ChessGame.hpp"

#include <string>

struct ImFont;

namespace Gui
{

    /// Renders the "Clock" panel: two player cards side by side, each
    /// showing the remaining time in a large readout and a bar whose fill
    /// is the fraction of the leading side's clock. The side to move is
    /// highlighted; a flagged side is drawn in red.
    ///
    /// `whiteEngineName` and `blackEngineName` are shown next to each
    /// card's label when non-empty. Empty strings mean "no engine on that
    /// side" or "engine has not yet reported its name," and the card
    /// renders without the extra text.
    void renderClockPanel(const ChessClock& clock, const ChessGame& game, ImFont* clockFont, ImFont* engineNameFont,
                          const std::string& whiteEngineName, const std::string& blackEngineName);

} // namespace Gui