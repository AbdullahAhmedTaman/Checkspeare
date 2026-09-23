#pragma once

#include "BoardGeometry.hpp"
#include "ChessClock.hpp"
#include "ChessGame.hpp"
#include "Config.hpp"
#include "GuiGameController.hpp"
#include "TextureLibrary.hpp"
#include "Theme.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <cstddef>

/// Owns the window, the game loop, and the top-level game state.
/// Non-copyable: it holds a reference to Config and the TextureLibrary,
/// both of which must outlive it.
class Application
{
public:
    Application(Config& conf, const Assets::TextureLibrary& assets);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void processEvents();
    void update(float dtSeconds);
    void render();

    Config& conf_;
    const Assets::TextureLibrary& assets_;

    sf::RenderWindow window_;
    sf::Clock deltaClock_;

    ChessGame game_;
    Gui::GuiGameController controller_;
    Gui::BoardGeometry geo_;
    Gui::BoardTheme theme_;

    /// Tracks the game cursor across frames so Application can detect when
    /// a move was committed and notify the clock (increment + side flip).
    std::size_t lastCursor_ = 0;

    /// The game clock. Declared after game_ so destruction order is sane
    /// even if the clock ever takes a reference to the game.
    ChessClock clock_;
};