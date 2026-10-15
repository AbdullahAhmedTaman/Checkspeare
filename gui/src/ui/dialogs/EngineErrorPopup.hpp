#pragma once

#include <string>

namespace Gui
{

    /// Renders a modal error popup when an engine fails to start. The
    /// popup shows `message` and clears it when the user clicks OK, so
    /// the caller can test `message.empty()` to know whether to keep
    /// calling this function.
    ///
    /// `message` is taken by reference so the function can clear it on
    /// dismissal without an additional API surface.
    void renderEngineErrorPopup(std::string& message);

} // namespace Gui