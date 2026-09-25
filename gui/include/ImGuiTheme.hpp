#pragma once

namespace Gui
{

    /// Applies the chess GUI's visual theme to the current ImGui context.
    /// Call once, after ImGui::SFML::Init and before any panel is submitted.
    void applyChessTheme();

} // namespace Gui