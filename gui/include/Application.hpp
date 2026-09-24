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

namespace Gui
{
    struct InputEvent;
    struct EngineThinkingSnapshot;
} // namespace Gui

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

    /// Advances game state: queued commands, engine, annotations, clock,
    /// cursor-driven animation. Must run before stepUi.
    void stepSimulation(float dtSeconds);

    /// Advances UI state and draws panels. Runs the ImGui frame.
    void stepUi(float dtSeconds);

    Gui::EngineThinkingSnapshot buildThinkingSnapshot() const;

    /// Decides whether an input event reaches the controller. True for
    /// FocusLost and drag-ending releases unconditionally; otherwise
    /// false when the engine controls the side to move, and gated by
    /// board-panel containment and ImGui mouse capture.
    bool shouldForwardInput(const Gui::InputEvent& ev) const;

    Config& conf_;
    const Assets::TextureLibrary& assets_;

    sf::RenderWindow window_;
    sf::Clock deltaClock_;

    ChessGame game_;
    ChessClock clock_;
    UciEngine engine_;
    EnginePlayer enginePlayer_{engine_};
    Gui::GuiGameController controller_;

    /// Recomputed each frame by BoardPanel::render. Describes the texture
    /// the board is drawn into, not the on-screen widget. Input handling
    /// reads the previous frame's value (one frame stale during a resize).
    Gui::BoardGeometry geo_;

    Gui::BoardTheme theme_;
    Gui::EvalBar evalBar_;
    Gui::BoardPanel boardPanel_;

    /// Tracks the game cursor across frames so Application can detect when
    /// a move was committed and notify the clock (increment + side flip).
    std::size_t lastCursor_ = 0;

    /// Borrowed from ImGui's font atlas; valid while the ImGui context lives.
    ImFont* clockFont_ = nullptr;

    /// Owned. Used to draw a1-h8 / 1-8 labels along the board edges.
    sf::Font coordinateFont_;

    /// Visual slide of the most recently committed ply. Started by
    /// stepSimulation when the cursor advances exactly one ply.
    Gui::MoveAnimation animation_;
};