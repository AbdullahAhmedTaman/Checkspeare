#pragma once

#include "../assets/TextureLibrary.hpp"

#include <string>

namespace Gui
{

    /// Resolved theme: texture keys into an TextureLibrary, not raw pointers.
    /// Safe to copy and store without worrying about the manager's lifetime.
    struct BoardTheme
    {
        std::string lightKey;
        std::string darkKey;
    };

    /// Builds a board theme for `tag`. Validates the keys against `manager`
    /// so missing assets fail at startup rather than on first draw.
    /// Throws std::runtime_error for unknown tags.
    BoardTheme getBoardTheme(const std::string& tag, const Assets::TextureLibrary& manager);

} // namespace Gui