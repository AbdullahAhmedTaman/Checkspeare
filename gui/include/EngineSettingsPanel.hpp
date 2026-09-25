#pragma once

#include "Config.hpp"
#include "EnginePlayer.hpp"
#include "UciEngine.hpp"

namespace Gui
{

    /// Renders the "Engine Settings" panel. Two tabs, one per side, each
    /// showing the binary path, start/stop controls, and status.
    ///
    /// `pathsLocked` disables the path text fields. Set while a game is in
    /// progress so the engine binary cannot be swapped mid-game; the setup
    /// modal is the only place paths can be chosen then.
    void renderEngineSettingsPanel(Config& conf, EnginePlayer& whitePlayer, UciEngine& whiteEngine, EnginePlayer& blackPlayer,
                                   UciEngine& blackEngine, bool pathsLocked);

} // namespace Gui