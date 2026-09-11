#pragma once
#include "AssetManager.h"
#include "SFML/Graphics/RenderWindow.hpp"
#include "Theme.h"
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

namespace BoardRenderer
{

    struct BoardGeometry
    {
        float squareSize = 50.0f;
        sf::Vector2f boardOrigin = {50.0f, 50.0f};
    };

    class BoardRenderer
    {
    public:
        BoardRenderer() = default;
        void render(sf::RenderWindow& window, const BoardGeometry& geo, const Theme::BoardTheme& theme,
                    const Chess::Board& board, const AssetManager::AssetManager& assets, bool isDragging,
                    Chess::Square draggedSquare, sf::Vector2f dragOffset);
    };
} // namespace BoardRenderer