#pragma once

#include "AssetsManager.h"
#include "BoardGeometry.hpp"
#include "ChessGame.hpp"
#include "Config.h"
#include "GuiGameController.hpp"
#include "Theme.h"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

class Application
{
public:
    Application(Config& conf, const Assets::AssetManager& assets);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void processEvents();
    void update(float dtSeconds);
    void render();

    Config& conf_;
    const Assets::AssetManager& assets_;

    sf::RenderWindow window_;
    sf::Clock deltaClock_;

    ChessGame game_;
    Gui::GuiGameController controller_;
    Gui::BoardGeometry geo_;
    Gui::BoardTheme theme_;
};