#pragma once

#include "AssetsManager.h"

#include <string>

namespace Gui
{

    /// Resolved theme: texture keys into an AssetManager, *not* raw pointers.
    ///
    /// Safe to copy, store, or hand across module boundaries without caring
    /// about the AssetManager's lifetime. Resolution to an actual texture
    /// happens at draw time, where a live manager is already required.
    struct BoardTheme
    {
        std::string lightKey;
        std::string darkKey;
    };

    /// Builds a board theme for the given tag.
    /// Eagerly validates the keys against `manager` so missing assets fail
    /// at startup rather than on the first frame.
    /// Throws std::runtime_error for unknown tags.
    BoardTheme getBoardTheme(const std::string& tag, const Assets::AssetManager& manager);

} // namespace Gui