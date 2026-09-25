#include "Application.hpp"

#include "Dockspace.hpp"
#include "EngineSettingsPanel.hpp"
#include "EngineThinkingPanel.hpp"
#include "GameOverModal.hpp"
#include "GameOverTransition.hpp"
#include "GameResult.hpp"
#include "ImGuiLayer.hpp"
#include "ImGuiTheme.hpp"
#include "InputEvent.hpp"
#include "MoveListPanel.hpp"
#include "SfmlInputAdapter.hpp"

#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <stdexcept>

Application::Application(Config& conf, const Assets::TextureLibrary& assets)
    : conf_(conf),
      assets_(assets),
      window_(sf::VideoMode({1280, 800}), "Chess GUI"),
      controller_(game_),
      theme_(Gui::getBoardTheme("Default", assets))
{
    window_.setFramerateLimit(60);

    // Clock is configured but not started. It stays stopped until a setup
    // is committed, so no time is lost while the user reads the modal.
    clock_.configure(conf_.timeControl);
    clock_.stop();

    // Clock snapshot wiring.
    //
    // The game carries a ClockSnapshot per ply plus one for the start
    // position. Whenever the cursor moves via navigation (undo, redo,
    // gotoPly, etc.), it calls this callback with the snapshot that
    // applies to the new cursor, and the clock restores it.
    //
    // Note: ChessGame::play does NOT fire this callback. A freshly-played
    // move is handled by Application::stepSimulation instead, because the
    // "after" snapshot for that ply only exists once the clock has been
    // told about the move (increment added, side flipped).
    game_.setClockSync(
        [this](const ClockSnapshot& s)
        {
            clock_.restore(s);
        });

    // Snapshot the initial clock state for ply 0.
    game_.setInitialClockSnapshot(clock_.snapshot());

    // A fresh move commit is the only event that mutates the clock
    // (increment + active side). The "after" snapshot for that ply can
    // only be taken once that mutation has happened, so it's recorded
    // here rather than in stepSimulation.
    game_.setMoveCommitted(
        [this](std::size_t ply, Color mover)
        {
            clock_.onMovePlayed(mover);
            game_.setClockSnapshotAt(ply, clock_.snapshot());
        });

    if (!ImGui::SFML::Init(window_, false))
        throw std::runtime_error("[ImGui] Initialization failed!");

    ImGui::GetIO().IniFilename = nullptr;

    ImGuiIO& io = ImGui::GetIO();

    ImFont* uiFont = io.Fonts->AddFontFromFileTTF("gui/assets/fonts/Roboto-Regular.ttf", 18.0f);
    IM_ASSERT(uiFont != nullptr && "[Fonts] Failed to load UI font!");

    clockFont_ = ImGui::GetIO().Fonts->AddFontFromFileTTF("gui/assets/fonts/Roboto-Bold.ttf", 36.0f);

    if (clockFont_ == nullptr)
        throw std::runtime_error("[Fonts] Failed to load clock font!");

    ImGui::SFML::UpdateFontTexture();

    Gui::applyChessTheme();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (!coordinateFont_.openFromFile("gui/assets/fonts/Roboto-Regular.ttf"))
        throw std::runtime_error("[Fonts] Failed to load board coordinate font!");

    // Seed the setup modal from Config. The user can change every field;
    // committed values are written back into Config so the next session
    // remembers them.
    pendingSetup_.timeControl = conf_.timeControl;
    pendingSetup_.whiteKind = Gui::PlayerKind::Human;
    pendingSetup_.blackKind = Gui::PlayerKind::Engine;
    pendingSetup_.whiteEnginePath = conf_.whiteEnginePath;
    pendingSetup_.blackEnginePath = conf_.blackEnginePath;

    // Neither engine is started here. The setup modal drives their
    // lifecycle; a missing or invalid binary no longer blocks the GUI
    // from opening.
}

Application::~Application()
{
    ImGui::SFML::Shutdown();
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

void Application::processEvents()
{
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

        // Clicking anywhere outside the board clears a pending selection.
        // This runs before shouldForwardInput because ImGui-captured
        // panel clicks are otherwise not forwarded, and the controller
        // would never see the click that should reset its state.
        if (inputEvent->kind == Gui::InputEvent::Kind::PointerPressed && inputEvent->button == Gui::MouseButton::Left &&
            !boardPanel_.contains(inputEvent->position))
        {
            controller_.cancelSelection();
        }

        if (!shouldForwardInput(*inputEvent))
            continue;

        Gui::InputEvent forwarded = *inputEvent;
        if (forwarded.kind != Gui::InputEvent::Kind::FocusLost)
            forwarded.position = boardPanel_.toBoardSpace(forwarded.position);

        const bool wasDragging = controller_.drag().active;
        const std::size_t cursorBefore = game_.cursor();

        controller_.handleEvent(forwarded, geo_);

        if (wasDragging && forwarded.kind == Gui::InputEvent::Kind::PointerReleased)
        {
            const std::size_t cursorAfter = game_.cursor();
            if (cursorAfter > cursorBefore)
                suppressAnimationAtPly_ = cursorAfter;
        }
    }
}

bool Application::shouldForwardInput(const Gui::InputEvent& ev) const
{
    if (ev.kind == Gui::InputEvent::Kind::FocusLost)
        return true;

    const bool endsDrag = ev.kind == Gui::InputEvent::Kind::PointerReleased && controller_.drag().active;
    if (endsDrag)
        return true;

    if (isGameOver())
        return false;

    // Never let the human play for an engine-controlled side.
    if (sideIsEngineControlled(game_.sideToMove()))
        return false;

    if (boardPanel_.contains(ev.position))
        return true;

    return !ImGui::GetIO().WantCaptureMouse;
}

bool Application::isGameOver() const
{
    return (game_.result().kind != GameResultKind::Ongoing) || clock_.flagFallen(WHITE) || clock_.flagFallen(BLACK);
}

void Application::update(float dtSeconds)
{
    stepSimulation(dtSeconds);
    stepUi(dtSeconds);
}

bool Application::sideIsEngineControlled(Color side) const
{
    // A side is blocked from human input only when an engine is actually
    // running and configured to play it. Configuration alone is a
    // preference; starting an engine is what makes it take effect. This
    // lets the user open the app, play, and start an engine mid-game
    // without the two states contradicting each other.
    const bool whiteWouldPlay = playerWhite_.isSideControlled(side) && engineWhite_.isRunning();
    const bool blackWouldPlay = playerBlack_.isSideControlled(side) && engineBlack_.isRunning();
    return whiteWouldPlay || blackWouldPlay;
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
        newGame();

    controller_.applyQueuedActions();

    const bool setupOpen = setupModalOpen_;

    if (!setupOpen)
    {
        playerWhite_.tick(game_, clock_);
        playerBlack_.tick(game_, clock_);
    }

    controller_.updateAnnotations();

    const bool gameOver = isGameOver();

    const std::size_t curr = game_.cursor();

    // Animation
    if (curr == lastCursor_ + 1)
    {
        if (curr != suppressAnimationAtPly_)
        {
            const MoveRecord& rec = game_.history()[curr - 1];
            animation_.start(rec.move.from(), rec.move.to(), game_.pieceAt(rec.move.to()));
        }
    }
    else if (curr + 1 == lastCursor_ && curr < game_.history().size())
    {
        const MoveRecord& rec = game_.history()[curr];
        animation_.start(rec.move.to(), rec.move.from(), game_.pieceAt(rec.move.from()));
    }
    else if (curr != lastCursor_)
    {
        animation_.active = false;
    }

    suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();

    animation_.tick(dtSeconds);

    // Cursor bookkeeping
    //
    // The clock is no longer advanced here. ChessGame::play announces a
    // fresh commit through moveCommitted_, which applies the increment,
    // flips the active side, and stores the resulting "after" snapshot
    // for that ply. Every other cursor change (undo, redo, gotoPly,
    // gotoEnd, a history-row click) already pushed the matching snapshot
    // through clockSync_, which *restores* it. Treating a redo as a
    // commit here is what re-applied increments on top of the restored
    // snapshot and flipped the active side a second time.
    lastCursor_ = curr;

    if (gameOver && !gameOverWas_)
    {
        playerWhite_.abort();
        playerBlack_.abort();
        controller_.cancelSelection();
        controller_.reopenGameOverModal();
    }
    gameOverWas_ = gameOver;

    if (!gameOver && !setupOpen)
        clock_.update(dtSeconds);
}

void Application::stepUi(float dtSeconds)
{
    const Gui::EngineThinkingSnapshot snap = buildThinkingSnapshot();

    if (snap.scoreMate)
        evalBar_.setTarget(0, *snap.scoreMate);
    else if (snap.scoreCp)
        evalBar_.setTarget(*snap.scoreCp, 0);
    evalBar_.update(dtSeconds);

    ImGui::SFML::Update(window_, sf::seconds(dtSeconds));
    Gui::beginMainDockspace();

    boardPanel_.render(geo_, theme_, game_.board(), assets_, coordinateFont_, controller_.drag(), controller_.pendingPromotion(),
                       controller_.annotations(), animation_);

    Gui::renderEngineThinkingPanel(snap, evalBar_);
    // Gui::renderEngineSettingsPanel(conf_, playerWhite_, engineWhite_, playerBlack_, engineBlack_,
    //                                /*pathsLocked=*/!setupModalOpen_);
    Gui::renderImGui(window_, conf_, geo_, controller_, game_, clock_, clockFont_);
    Gui::renderGameOverModal(controller_, game_, clock_, setupModalOpen_);

    Gui::renderMoveListPanel(game_, controller_);
    if (auto committed = Gui::renderGameSetupModal(pendingSetup_, setupModalOpen_))
    {
        applyGameSetup(*committed);
        setupModalOpen_ = false;
    }
}

Gui::EngineThinkingSnapshot Application::buildThinkingSnapshot() const
{
    Gui::EngineThinkingSnapshot snap;

    // Pick which engine's info to display:
    //   - If exactly one engine is running, show it regardless of whose
    //     turn it is. A human playing against an engine wants to see that
    //     engine's evaluation while they think, not a blank panel.
    //   - If both are running, show the one whose side is to move.
    //   - If neither is running, the snapshot stays empty and the panel
    //     shows Idle.
    //
    // "Running" requires both a configured side and a live process: a
    // preference without a started engine is not a reason to show its
    // (empty) search info.
    const bool whiteActive = engineWhite_.isRunning() && playerWhite_.isSideControlled(WHITE);
    const bool blackActive = engineBlack_.isRunning() && playerBlack_.isSideControlled(BLACK);

    const EnginePlayer* active = nullptr;
    if (whiteActive && blackActive)
        active = (game_.sideToMove() == WHITE) ? &playerWhite_ : &playerBlack_;
    else if (whiteActive)
        active = &playerWhite_;
    else if (blackActive)
        active = &playerBlack_;

    if (active == nullptr)
        return snap;

    // `activeSide` names the engine being shown, not the side to move.
    // When the human is on the clock against a running engine, the panel
    // still reads "White" (or "Black") — meaning "this is White's engine."
    snap.activeSide = (active == &playerWhite_) ? "White" : "Black";
    snap.thinking = active->thinking();

    if (const auto& info = active->lastInfo())
    {
        snap.depth = info->depth;
        snap.seldepth = info->seldepth;
        snap.nodes = info->nodes;
        snap.nps = info->nps;

        // Normalize to White's perspective for display. `lastInfoSide_`
        // records which side produced the info, so this works whether the
        // active player happens to be White or Black.
        const bool fromBlack = (active->lastInfoSide() == BLACK);
        if (info->scoreCp)
            snap.scoreCp = fromBlack ? -(*info->scoreCp) : *info->scoreCp;
        if (info->scoreMate)
            snap.scoreMate = fromBlack ? -(*info->scoreMate) : *info->scoreMate;
    }

    return snap;
}

void Application::render()
{
    window_.clear(sf::Color::White);
    ImGui::SFML::Render(window_);
    window_.display();
}

void Application::newGame()
{
    // Defer the reset to applyGameSetup. The modal needs the old game to
    // still be alive while it is on screen; resetting first would blank
    // the board behind it. A failed or abandoned commit also cannot leave
    // the game half-reset, because nothing changes until Start is pressed.
    setupModalOpen_ = true;
    controller_.updateAnnotations();
}

void Application::applyGameSetup(const Gui::GameSetup& setup)
{
    // Snapshot the current state into Config so the next session can seed
    // the modal from these choices.
    conf_.timeControl = setup.timeControl;
    conf_.whiteEnginePath = setup.whiteEnginePath;
    conf_.blackEnginePath = setup.blackEnginePath;

    // Tear down the old game before building the new one.
    playerWhite_.abort();
    playerBlack_.abort();
    engineWhite_.stop();
    engineBlack_.stop();

    game_.reset();
    lastCursor_ = 0;
    gameOverWas_ = false;
    animation_.active = false;
    suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();

    // Configure and start the clock. The snapshot taken immediately after
    // start() becomes the "ply 0" state the clock restores to on undo.
    clock_.configure(setup.timeControl);
    clock_.start();
    game_.setInitialClockSnapshot(clock_.snapshot());

    // Player kinds.
    playerWhite_.setSideControlled(WHITE, setup.whiteKind == Gui::PlayerKind::Engine);
    playerBlack_.setSideControlled(BLACK, setup.blackKind == Gui::PlayerKind::Engine);

    // Engines: launched for engine sides, stopped for human sides.
    launchOrStopEngine(engineWhite_, setup.whiteKind, setup.whiteEnginePath);
    launchOrStopEngine(engineBlack_, setup.blackKind, setup.blackEnginePath);

    controller_.updateAnnotations();
    controller_.reopenGameOverModal();
}

void Application::launchOrStopEngine(UciEngine& engine, Gui::PlayerKind kind, const std::string& path)
{
    if (kind != Gui::PlayerKind::Engine)
    {
        engine.stop();
        return;
    }

    try
    {
        engine.start(path);
    }
    catch (const std::exception&)
    {
        // A bad path or a failed spawn leaves the engine stopped. The
        // Engine Settings panel shows Not running and offers Start, so
        // the user can recover without restarting the GUI.
    }
}