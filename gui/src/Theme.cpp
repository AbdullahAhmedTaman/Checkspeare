#include "Theme.h"

#include <stdexcept>
#include <string>

namespace Gui
{

    BoardTheme getBoardTheme(const std::string& tag, const Assets::AssetManager& manager)
    {
        BoardTheme theme;

        if (tag == "Default")
        {
            theme.lightKey = Assets::Key::SquareWhite;
            theme.darkKey = Assets::Key::SquareBlack;
        }
        else if (tag == "Modern")
        {
            throw std::runtime_error("[Theme Error] No implementation for tag: " + tag);
        }
        else
        {
            throw std::runtime_error("[Theme Error] Unknown theme tag: " + tag);
        }

        // Validate now: getTexture throws std::out_of_range if the key is
        // missing, so an asset-loading mistake surfaces at construction.
        (void) manager.getTexture(theme.lightKey);
        (void) manager.getTexture(theme.darkKey);

        return theme;
    }

} // namespace Gui