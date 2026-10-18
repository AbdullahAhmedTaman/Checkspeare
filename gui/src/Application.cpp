#include "Application.hpp"

#include "game/GameResult.hpp"
#include "input/InputEvent.hpp"
#include "input/SfmlInputAdapter.hpp"
#include "ui/ClockPanel.hpp"
#include "ui/DockSpace.hpp"
#include "ui/FenPanel.hpp"
#include "ui/ImGuiTheme.hpp"
#include "ui/MoveListPanel.hpp"
#include "ui/StatusPanel.hpp"
#include "ui/dialogs/EngineErrorPopup.hpp"
#include "ui/dialogs/GameOverModal.hpp"
#include "ui/engine/EngineThinkingPanel.hpp"

#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <stdexcept>

namespace
{
    sf::ContextSettings makeWindowSettings()
    {
        sf::ContextSettings s;
        s.antiAliasingLevel = 8;
        return s;
    }
} // namespace

Application::Application(Config& conf, const Assets::TextureLibrary& assets, const Assets::SoundLibrary& sounds)
    : assets_(assets),
      sounds_(sounds),
      window_(sf::VideoMode({1150, 1000}), "Checkspeare GUI", sf::Style::Default, sf::State::Windowed, makeWindowSettings()),
      controller_(game_),
      theme_(Gui::getBoardTheme("Default", assets)),
      soundPlayer_(sounds),
      soundDirector_(soundPlayer_),
      setupFlow_(conf, game_, clock_, engines_, controller_, geo_)
{
    window_.setVerticalSyncEnabled(true);
    sf::Image icon;

    if (!icon.loadFromFile(RESOURCES_PATH "/chess.png"))
    {
        throw std::runtime_error("Failed to load application icon\n");
    }
    else
    {
        window_.setIcon(icon);
    }

    // Clock is configured but not started. It stays stopped until a setup
    // is committed, so no time is lost while the user reads the modal.
    clock_.configure(conf.timeControl);
    clock_.stop();

    wireClockCallbacks();

    // Move authority lives in the controller, at the point a move is about
    // to be committed. Everything upstream (input routing in
    // processEvents/shouldForwardInput) is now free to deliver events to
    // the board even while an engine controls the side to move; non-mutating
    // gestures (arrows, clear-arrows on empty-square click, hover) work
    // during engine thinking, and the controller itself rejects a move
    // attempt from the wrong side.
    controller_.setHumanMayMove(
        [this](Color side)
        {
            return !engines_.sideIsEngineControlled(side);
        });

    // Engine ownership is a query over the engine players, which only
    // Application can see. The controller consults this for canResign()
    // and sideIsHuman(); it does not need to know what an engine is.
    controller_.setSideIsEngineControlled(
        [this](Color side)
        {
            return engines_.sideIsEngineControlled(side);
        });

    soundDirector_.setSideIsHuman(
        [this](Color side)
        {
            return sideIsHuman(side);
        });

    // Application's per-frame bookkeeping is reset whenever a setup is
    // committed: the new game starts at ply 0, and any previous game-over
    // latch must be cleared so the transition detector can fire again.
    setupFlow_.setOnApplied(
        [this]
        {
            moveAnimator_.reset(0);
            gameOverWas_ = false;
        });

    if (!ImGui::SFML::Init(window_, false))
        throw std::runtime_error("[ImGui] Initialization failed!");

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    loadFonts();
    ImGui::SFML::UpdateFontTexture();

    Gui::applyChessTheme();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (!coordinateFont_.openFromFile(RESOURCES_PATH "fonts/Roboto-Regular.ttf"))
        throw std::runtime_error("[Fonts] Failed to load board coordinate font!");

    setupFlow_.seedFromConfig();
}

Application::~Application()
{
    ImGui::SFML::Shutdown();
}

void Application::wireClockCallbacks()
{
    // The game carries a clock snapshot per ply. Navigation methods fire
    // clockSync_, which restores the matching snapshot. ChessGame::play
    // does not fire it; a fresh commit goes through moveCommitted_ instead,
    // since the "after" snapshot for a ply only exists once the clock has
    // been told about the move (increment added, side flipped).
    game_.setClockSync(
        [this](const ClockSnapshot& s)
        {
            clock_.restore(s);
        });

    game_.setInitialClockSnapshot(clock_.snapshot());

    game_.setMoveCommitted(
        [this](std::size_t ply, Color mover)
        {
            clock_.onMovePlayed(mover);
            game_.setClockSnapshotAt(ply, clock_.snapshot());

            // Sound category was classified at commit time and stored in
            // the record. Playing here catches both human and engine moves,
            // since every commit routes through ChessGame::play.
            soundDirector_.playMoveSound(game_.history()[ply].sound);
        });
}

void Application::loadFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    ImFont* uiFont = io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fonts/Roboto-Medium.ttf", 19.0f);
    IM_ASSERT(uiFont != nullptr && "[Fonts] Failed to load UI font!");

    clockFont_ = io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fonts/Roboto-Bold.ttf", 36.0f);
    if (clockFont_ == nullptr)
        throw std::runtime_error("[Fonts] Failed to load clock font!");

    engineNameFont_ = io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fonts/Roboto-Bold.ttf", 26.0f);
    if (engineNameFont_ == nullptr)
        throw std::runtime_error("[Fonts] Failed to load engine name font!");

    moveListFont_ = io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fonts/Roboto-Bold.ttf", 20.0f);
    if (moveListFont_ == nullptr)
        throw std::runtime_error("[Fonts] Failed to load move list font!");
}

void Application::run()
{
    while (window_.isOpen())
    {
        processEvents();

        const float dt = deltaClock_.restart().asSeconds();
        update(dt);
        render();
    }
}

void Application::update(float dtSeconds)
{
    stepSimulation(dtSeconds);
    stepUi(dtSeconds);
}

void Application::render()
{
    window_.clear(sf::Color::White);
    ImGui::SFML::Render(window_);
    window_.display();
}

void Application::processEvents()
{
    // Read the two gate conditions once per frame. Nothing inside the loop
    // mutates them, so this is equivalent to checking per event and avoids
    // repeated game_.result() / clock_.flagFallen(...) calls.
    const bool setupOpen = setupFlow_.isOpen();
    const bool gameOver = isGameOver();

    while (const std::optional event = window_.pollEvent())
    {
        ImGui::SFML::ProcessEvent(window_, *event);

        if (event->is<sf::Event::Closed>())
        {
            window_.close();
            continue;
        }

        const auto inputEvent = Gui::toInputEvent(*event);
        if (!inputEvent)
            continue;

        const bool insideBoard = boardPanel_.contains(inputEvent->position);

        // Clicking anywhere outside the board clears a pending selection.
        // Runs before shouldForwardInput because ImGui-captured panel
        // clicks are otherwise not forwarded. Skipped while a modal is
        // open: the modal owns every click.
        if (!setupOpen && !gameOver && inputEvent->kind == Gui::InputEvent::Kind::PointerPressed &&
            inputEvent->button == Gui::MouseButton::Left && !insideBoard)
        {
            controller_.cancelSelection();
        }

        // The controller decides whether it wants right-click events that
        // land outside the board: presses are dropped, releases are let
        // through only when they resolve an in-flight arrow gesture.
        if (!controller_.wantsEvent(*inputEvent, insideBoard))
            continue;

        if (inputEvent->kind == Gui::InputEvent::Kind::KeyPressed)
        {
            if (setupOpen || gameOver)
                continue;

            if (ImGui::GetIO().WantCaptureKeyboard)
                continue;

            controller_.handleEvent(*inputEvent, geo_);
            continue;
        }

        if (!shouldForwardInput(*inputEvent, setupOpen, gameOver))
            continue;

        Gui::InputEvent forwarded = *inputEvent;
        if (forwarded.kind != Gui::InputEvent::Kind::FocusLost)
            forwarded.position = boardPanel_.toBoardSpace(forwarded.position);

        controller_.handleEvent(forwarded, geo_);
    }
}

bool Application::shouldForwardInput(const Gui::InputEvent& ev, bool setupOpen, bool gameOver) const
{
    if (ev.kind == Gui::InputEvent::Kind::FocusLost)
        return true;

    const bool endsDrag = ev.kind == Gui::InputEvent::Kind::PointerReleased && controller_.drag().active;
    if (endsDrag)
        return true;

    // Modals gate everything else. ImGui's WantCaptureMouse alone is not
    // enough: the board is itself an ImGui panel, so a click over its
    // rectangle would otherwise be forwarded regardless of what is on top.
    //
    // Move authority is intentionally *not* checked here. The controller
    // consults humanMayMove_ at the point a move is committed, which lets
    // non-mutating gestures (arrows, clear-arrows, hover) reach the board
    // even while an engine controls the side to move.
    if (setupOpen || gameOver)
        return false;

    if (boardPanel_.contains(ev.position))
        return true;

    return !ImGui::GetIO().WantCaptureMouse;
}

bool Application::isGameOver() const
{
    return (game_.result().kind != GameResultKind::Ongoing) || clock_.flagFallen(WHITE) || clock_.flagFallen(BLACK);
}

bool Application::canResign() const
{
    if (isGameOver())
        return false;
    return controller_.canResign();
}

bool Application::sideIsHuman(Color side) const
{
    return controller_.sideIsHuman(side);
}

std::string Application::applyLoadFen(const std::string& fen)
{
    // Abort both engines. An engine mid-search is analyzing a position
    // that is about to stop existing; its next "bestmove" would be
    // applied to the new board and either be illegal or be played in a
    // position it never saw. abortAll clears the search identity
    // (thinkStartPly_, lastInfo_, pendingBestMove_), so a late bestmove
    // cannot be mistaken for a result on the new position.
    //
    // The next tick will re-evaluate from scratch and, if it's still the
    // engine's turn, start a fresh search on the loaded position.
    engines_.abortAll();

    try
    {
        game_.loadFEN(fen);
    }
    catch (const std::exception& e)
    {
        return e.what();
    }

    moveAnimator_.reset(game_.cursor());

    controller_.clearArrows();
    controller_.cancelSelection();
    controller_.updateAnnotations();

    gameOverWas_ = false;

    // Stop the clock. The side to move may have flipped, and the clock
    // has no way to know that without a move being played; leaving it
    // running would tick the wrong side. The user can start a fresh
    // timed game from the setup modal if they want a clock.
    clock_.stop();

    return {};
}

void Application::stepSimulation(float dtSeconds)
{
    // Quit first: if the user clicked Quit in the modal, do it before
    // anything else this frame.
    if (controller_.consumeQuitRequest())
    {
        window_.close();
        return;
    }

    if (controller_.consumeNewGameRequest())
        setupFlow_.requestNewGame();

    if (auto fen = controller_.consumeLoadFenRequest())
    {
        // applyLoadFen aborts both engines, so this must run before the
        // engine tick below: they should see the new position and the
        // aborted (not-thinking) state on the same frame.
        fenLoadError_ = applyLoadFen(*fen);
    }

    if (auto resigner = controller_.consumeResignRequest())
        game_.resign(*resigner);

    controller_.applyQueuedActions();

    // Input-feedback sounds. These outcomes have no MoveRecord counterpart
    // (a rejected or abandoned move is never committed), so they are keyed
    // off the controller's ActionResult instead. SoundDirector latches the
    // kind against the previous frame to avoid re-firing every frame.
    soundDirector_.onActionResult(controller_.result().kind);

    const bool setupOpen = setupFlow_.isOpen();

    if (!setupOpen)
        engines_.tick(game_, clock_);

    controller_.updateAnnotations();

    const bool gameOver = isGameOver();
    const std::size_t curr = game_.cursor();

    moveAnimator_.update(game_, curr, controller_.lastMoveWasDrag(), dtSeconds);

    if (gameOver && !gameOverWas_)
    {
        engines_.abortAll();
        controller_.cancelSelection();
        controller_.reopenGameOverModal();
        soundDirector_.playGameOverSound(game_, clock_);
    }
    gameOverWas_ = gameOver;

    if (!gameOver && !setupOpen)
        clock_.update(dtSeconds);
}

void Application::stepUi(float dtSeconds)
{
    const Gui::EngineThinkingSnapshot snap = engines_.buildThinkingSnapshot(game_);

    if (snap.scoreMate)
        evalBar_.setTarget(0, *snap.scoreMate);
    else if (snap.scoreCp)
        evalBar_.setTarget(*snap.scoreCp, 0);
    evalBar_.update(dtSeconds);

    ImGui::SFML::Update(window_, sf::seconds(dtSeconds));
    Gui::beginMainDockspace();

    boardPanel_.render(geo_, theme_, game_.board(), assets_, coordinateFont_, controller_.drag(), controller_.pendingPromotion(),
                       controller_.annotations(), moveAnimator_.animation(), controller_.arrows());

    Gui::renderEngineThinkingPanel(snap, evalBar_);
    Gui::renderStatusPanel(controller_, game_, canResign());
    Gui::renderGameOverModal(controller_, game_, clock_, setupFlow_.isOpen());

    // A side is labeled with its engine's name when an engine is running,
    // and "Human" otherwise. During the handshake window the name is still
    // empty, so "Engine" is shown rather than "Human" for a machine side.
    auto displayName = [](const UciEngine& engine) -> std::string
    {
        if (!engine.isRunning())
            return "Human";
        const std::string& n = engine.name();
        return n.empty() ? "Engine" : n;
    };

    Gui::renderClockPanel(clock_, game_, assets_, clockFont_, engineNameFont_, displayName(engines_.whiteEngine()),
                          displayName(engines_.blackEngine()));
    Gui::renderMoveListPanel(game_, controller_, moveListFont_);
    Gui::renderFenPanel(game_, controller_, fenLoadError_);
    Gui::renderEngineErrorPopup(engines_.error());

    // Renders the setup modal and, on commit, applies the setup through
    // SetupFlow. Application's bookkeeping reset rides on the onApplied
    // callback registered in the constructor.
    setupFlow_.render();
}