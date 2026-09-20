#include "Theme.hpp"

#include <stdexcept>
#include <string>

namespace Gui
{

    BoardTheme getBoardTheme(const std::string& tag, const Assets::TextureLibrary& manager)
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

        // Validate keys now so a missing asset fails at construction, not on
        // the first draw. getTexture throws std::out_of_range on missing keys.
        (void) manager.getTexture(theme.lightKey);
        (void) manager.getTexture(theme.darkKey);

        return theme;
    }

} // namespace Gui