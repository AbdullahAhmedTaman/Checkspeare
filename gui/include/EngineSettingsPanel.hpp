#pragma once

#include "Config.hpp"
#include "EnginePlayer.hpp"
#include "UciEngine.hpp"

namespace Gui
{

    /// Renders the "Engine Settings" panel. Two tabs, one per side, each
    /// showing the binary path, start/stop controls, and the side-control
    /// toggle for that engine.
    void renderEngineSettingsPanel(Config& conf, EnginePlayer& whitePlayer, UciEngine& whiteEngine, EnginePlayer& blackPlayer,
                                   UciEngine& blackEngine);

} // namespace Gui