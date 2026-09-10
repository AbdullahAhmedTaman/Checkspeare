#pragma once
#include "AssetManager.h"
#include <SFML/Graphics.hpp>

struct Config
{
    bool showDemoWindow = true;
    float squareSize = 80.0f;
};

namespace RenderUI
{
    sf::RenderWindow initWindow();
    void imGuiRenderUI(sf::RenderWindow& window, Config& conf);
    // where to write the imGui components
    void windowRenderUI(sf::RenderWindow& window, Config& conf, const AssetManager::AssetManager& assets);
} // namespace RenderUI