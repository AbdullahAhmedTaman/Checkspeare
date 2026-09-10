#include "../include/Theme.h"
#include <stdexcept>
#include <string>

namespace Theme
{
    BoardTheme getBoardTheme(const std::string& tag, const AssetManager::AssetManager& manager)
    {
        BoardTheme theme;
        if (tag == "Modern")
        {
            throw std::runtime_error("[Theme Error] no implementation for " + tag + "\n");
        }
        else
        {
            // Default
            theme.Square_Light = &manager.getTexture("Square_White");
            theme.Square_Dark = &manager.getTexture("Square_Black");
        }
        return theme;
    }
} // namespace Theme