#pragma once

#include "BoardGeometry.hpp"
#include "BoardPanel.hpp"
#include "ChessClock.hpp"
#include "ChessGame.hpp"
#include "Config.hpp"
#include "EnginePlayer.hpp"
#include "EvalBar.hpp"
#include "GameSetupPanel.hpp"
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

    /// Advances UI state and runs the ImGui frame.
    void stepUi(float dtSeconds);

    /// Decides whether an input event reaches the controller. True for
    /// FocusLost and drag-ending releases unconditionally; otherwise
    /// false when the engine controls the side to move, and gated by
    /// board-panel containment and ImGui mouse capture.
    bool shouldForwardInput(const Gui::InputEvent& ev) const;

    /// True when the game is over for any reason: game.result() is
    /// terminal, or either side's clock has flagged.
    bool isGameOver() const;

    /// True when the human side to move may resign.
    bool canResign() const;

    /// True if either engine is configured to play `side`.
    bool sideIsEngineControlled(Color side) const;

    /// Reopens the setup modal. The actual reset happens in applyGameSetup.
    void newGame();

    /// Resets game and clock for a fresh game from the committed setup.
    void applyGameSetup(const Gui::GameSetup& setup);

    /// Starts `engine` with `path` when `kind == Engine`, stops it
    /// otherwise. Swallows start failures; a bad path leaves the engine
    /// stopped and the setup panel can retry.
    static void launchOrStopEngine(UciEngine& engine, Gui::PlayerKind kind, const std::string& path);

    Gui::EngineThinkingSnapshot buildThinkingSnapshot() const;

    /// Wires the clock callbacks the game fires during navigation and
    /// commits. Called once from the constructor before any move is made.
    void wireClockCallbacks();

    /// Loads every UI font into the ImGui atlas. Caller is responsible for
    /// calling ImGui::SFML::UpdateFontTexture() afterwards.
    void loadFonts();

    /// Seeds the setup modal's edit buffer from Config. Called once at
    /// construction; subsequent new-game requests reuse the buffer.
    void seedSetupModalFromConfig();

    Config& conf_;
    const Assets::TextureLibrary& assets_;

    sf::RenderWindow window_;
    sf::Clock deltaClock_;

    ChessGame game_;
    ChessClock clock_;

    UciEngine engineWhite_;
    UciEngine engineBlack_;

    EnginePlayer playerWhite_{engineWhite_};
    EnginePlayer playerBlack_{engineBlack_};

    Gui::GuiGameController controller_;
    Gui::BoardGeometry geo_;
    Gui::BoardTheme theme_;
    Gui::EvalBar evalBar_;
    Gui::BoardPanel boardPanel_;

    /// Borrowed from ImGui's font atlas; valid while the ImGui context lives.
    ImFont* clockFont_ = nullptr;
    ImFont* engineNameFont_ = nullptr;
    ImFont* moveListFont_ = nullptr;

    /// Owned. Used for a1-h8 / 1-8 labels drawn into the board texture.
    sf::Font coordinateFont_;

    /// Tracks the game cursor across frames so Application can detect when
    /// a move was committed and notify the clock.
    std::size_t lastCursor_ = 0;

    /// True on frames where game_.result() reports a terminal state. Used
    /// to fire one-shot reactions (engine abort) on the transition into
    /// game-over and to keep the clock from ticking once there.
    bool gameOverWas_ = false;

    /// Visual slide of the most recently committed ply.
    Gui::MoveAnimation animation_;

    /// Cursor position of the ply committed by the most recent drag
    /// release. Suppresses the slide for that ply only.
    /// max() means "no suppression".
    std::size_t suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();

    /// Edit buffer for the setup modal, persisted across new-game requests.
    Gui::GameSetup pendingSetup_;

    /// True while the setup modal should be showing.
    bool setupModalOpen_ = true;
};