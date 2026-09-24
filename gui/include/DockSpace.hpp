#pragma once

namespace Gui
{

    /// Begins the main dockspace host window for this frame. Call once per
    /// frame, before any dockable panel is submitted.
    ///
    /// On the first call of the session the default layout is built, unless
    /// a layout was already restored from the ImGui .ini (detected by the
    /// dockspace node already being split).
    void beginMainDockspace();

} // namespace Gui