#pragma once

#include "BoardGeometry.hpp"
#include "BoardPanel.hpp"
#include "ChessClock.hpp"
#include "ChessGame.hpp"
#include "Config.hpp"
#include "EnginePlayer.hpp"
#include "EvalBar.hpp"
#include "GuiGameController.hpp"
#include "MoveAnimation.hpp"
#include "TextureLibrary.hpp"
#include "Theme.hpp"
#include "UciEngine.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <cstddef>

struct ImFont;

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
    ChessClock clock_;
    UciEngine engine_;
    EnginePlayer enginePlayer_{engine_};
    Gui::GuiGameController controller_;
    Gui::BoardGeometry geo_;
    Gui::BoardTheme theme_;
    Gui::EvalBar evalBar_;
    Gui::BoardPanel boardPanel_;
    /// Tracks the game cursor across frames so Application can detect when
    /// a move was committed and notify the clock (increment + side flip).
    std::size_t lastCursor_ = 0;
    ImFont* clockFont_ = nullptr;
    sf::Font coordinateFont_;

    /// Started when a single new ply is committed; cleared on any cursor
    /// change that is not a single forward step (undo, gotoPly, loadFEN).
    Gui::MoveAnimation animation_;
    /// Set in processEvents when a forwarded release ends an active drag.
    /// Consumed by the cursor-change block in update, then cleared, so it
    /// only ever affects the frame in which the release occurred.
    bool justEndedDrag_ = false;
};