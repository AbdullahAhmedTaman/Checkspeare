#pragma once

#include "Config.hpp"
#include "SetupFlow.hpp"
#include "assets/SoundLibrary.hpp"
#include "assets/TextureLibrary.hpp"
#include "audio/SoundDirector.hpp"
#include "audio/SoundPlayer.hpp"
#include "controller/GuiGameController.hpp"
#include "engine/EngineRig.hpp"
#include "game/BoardGeometry.hpp"
#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"
#include "game/MoveAnimator.hpp"
#include "ui/EvalBar.hpp"
#include "ui/Theme.hpp"
#include "ui/board/BoardPanel.hpp"


#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <string>

struct ImFont;

namespace Gui
{
    struct InputEvent;
} // namespace Gui

/// Owns the window, the game loop, and the top-level game state.
/// Non-copyable: it holds a reference to Config and the TextureLibrary,
/// both of which must outlive it.
class Application
{
public:
    Application(Config& conf, const Assets::TextureLibrary& assets, const Assets::SoundLibrary& sounds);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void processEvents();
    void update(float dtSeconds);
    void render();

    /// Decides whether an input event reaches the controller. True for
    /// FocusLost and drag-ending releases unconditionally; otherwise
    /// gated by board-panel containment and ImGui mouse capture. Modals
    /// block everything else. Move authority is enforced inside the
    /// controller, not here. `setupOpen` and `gameOver` are read once per
    /// frame by processEvents and passed in.
    bool shouldForwardInput(const Gui::InputEvent& ev, bool setupOpen, bool gameOver) const;

    /// Advances game state: queued commands, engine, annotations, clock,
    /// cursor-driven animation. Must run before stepUi.
    void stepSimulation(float dtSeconds);

    /// Advances UI state and runs the ImGui frame.
    void stepUi(float dtSeconds);

    /// True when the game is over for any reason: game.result() is
    /// terminal, or either side's clock has flagged.
    bool isGameOver() const;

    /// True when the human side to move may resign. Gates on isGameOver()
    /// (which includes clock flags) and then delegates to the controller,
    /// which knows the game result and the side's engine ownership.
    bool canResign() const;

    /// True if no engine is configured to play `side`. Hotseat and
    /// human-vs-human both resolve to "both sides human".
    bool sideIsHuman(Color side) const;

    /// Replaces the current position with `fen`. Aborts both engines (they
    /// may be mid-search on the old position), resets the board, and lets
    /// each EnginePlayer re-evaluate whether to start a fresh search. The
    /// clock is stopped: the side to move may have flipped, and the clock
    /// cannot know that without a move being played. Returns an error
    /// message on failure, empty on success.
    std::string applyLoadFen(const std::string& fen);

    /// Wires the clock callbacks the game fires during navigation and
    /// commits. Called once from the constructor before any move is made.
    void wireClockCallbacks();

    /// Loads every UI font into the ImGui atlas. Caller is responsible for
    /// calling ImGui::SFML::UpdateFontTexture() afterwards.
    void loadFonts();

    const Assets::TextureLibrary& assets_;
    const Assets::SoundLibrary& sounds_;

    sf::RenderWindow window_;
    sf::Clock deltaClock_;

    ChessGame game_;
    ChessClock clock_;
    EngineRig engines_;

    Gui::GuiGameController controller_;
    Gui::BoardGeometry geo_;
    Gui::BoardTheme theme_;
    Gui::EvalBar evalBar_;
    Gui::BoardPanel boardPanel_;
    SoundPlayer soundPlayer_;

    SoundDirector soundDirector_;
    MoveAnimator moveAnimator_;
    SetupFlow setupFlow_;

    /// Borrowed from ImGui's font atlas; valid while the ImGui context lives.
    ImFont* clockFont_ = nullptr;
    ImFont* engineNameFont_ = nullptr;
    ImFont* moveListFont_ = nullptr;

    /// Owned. Used for a1-h8 / 1-8 labels drawn into the board texture.
    sf::Font coordinateFont_;

    /// Error surface for FEN loads specifically. Rendered inline by the
    /// FEN panel so the user sees it next to the field they typed into.
    /// Cleared when the panel's Apply or Refresh runs.
    std::string fenLoadError_;

    /// True on frames where game_.result() reports a terminal state. Used
    /// to fire one-shot reactions (engine abort) on the transition into
    /// game-over and to keep the clock from ticking once there.
    bool gameOverWas_ = false;
};