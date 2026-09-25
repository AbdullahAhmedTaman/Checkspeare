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
#include <limits>

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

    /// True when the game is over for any reason: game.result() is
    /// terminal, or either side's clock has flagged.
    bool isGameOver() const;

    /// Resets game and clock for a fresh game. Called when the game-over
    /// banner's New Game button fires a request.
    void newGame();

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

    /// True on frames where game_.result() reports a terminal state.
    /// Used to fire one-shot reactions (engine abort) on the transition
    /// into game-over, and to keep the clock from ticking once there.
    bool gameOverWas_ = false;

    /// Borrowed from ImGui's font atlas; valid while the ImGui context lives.
    ImFont* clockFont_ = nullptr;

    /// Owned. Used to draw a1-h8 / 1-8 labels along the board edges.
    sf::Font coordinateFont_;

    /// Visual slide of the most recently committed ply. Started by
    /// stepSimulation when the cursor advances exactly one ply.
    Gui::MoveAnimation animation_;

    /// Cursor position of the ply committed by the most recent drag
    /// release. Suppresses the slide for that ply only, so an engine
    /// response arriving on a later frame is unaffected.
    /// max() means "no suppression".
    std::size_t suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();
};