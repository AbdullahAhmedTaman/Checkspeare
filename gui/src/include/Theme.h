#pragma once
#include "AssetManager.h"
#include "SFML/Graphics/Texture.hpp"
#include <SFML/Graphics.hpp>
#include <string>

namespace Theme
{
    // universal Theme Texture Holder
    // The output of the AssetManager
    struct BoardTheme
    {
        const sf::Texture* Square_Light;
        const sf::Texture* Square_Dark;
    };

    // Use it for IdiotProof code
    struct Tag
    {
        std::string Defualt = "Default";
    };

    BoardTheme getBoardTheme(const std::string& tag, const AssetManager::AssetManager& manager);

} // namespace Theme