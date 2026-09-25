#pragma once

#include "Config.hpp"
#include "EnginePlayer.hpp"
#include "UciEngine.hpp"

namespace Gui
{

    /// Renders the "Engine Settings" panel. Exposes the runtime knobs that
    /// are safe to toggle mid-session: which sides the engine plays, and
    /// an informational readout of the engine's handshake state.
    ///
    /// `enginePlayer` is non-const because the side toggles call
    /// setSideControlled. `engine` is const: the panel only reads status.
    void renderEngineSettingsPanel(Config& conf, EnginePlayer& enginePlayer, UciEngine& engine);

} // namespace Gui