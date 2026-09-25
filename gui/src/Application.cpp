#include "Application.hpp"

#include "Dockspace.hpp"
#include "EngineThinkingPanel.hpp"
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

    ChessClock::TimeControl tc;
    tc.initialMs = 60 * 1000;
    tc.incrementMs = 3 * 1000;
    clock_.configure(tc);
    clock_.start();

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

    /// Where Engine integration comes in :
    engine_.start(conf_.enginePath);
    enginePlayer_.setSideControlled(BLACK, true);
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
        if (!inputEvent || !shouldForwardInput(*inputEvent))
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

    if (enginePlayer_.isSideControlled(game_.sideToMove()))
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

    const bool gameOver = isGameOver();

    enginePlayer_.tick(game_, clock_);
    controller_.updateAnnotations();

    const std::size_t curr = game_.cursor();

    // ------------------------------------------------------------------
    // Animation
    // ------------------------------------------------------------------
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

    if (justEnteredGameOver(gameOver, gameOverWas_))
    {
        enginePlayer_.abort();
        controller_.clearHints();
        controller_.reopenGameOverModal();
    }

    if (!gameOver)
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
    Gui::renderImGui(window_, conf_, geo_, controller_, game_, clock_, clockFont_);
    Gui::renderMoveListPanel(game_, controller_);
}

Gui::EngineThinkingSnapshot Application::buildThinkingSnapshot() const
{
    Gui::EngineThinkingSnapshot snap;
    snap.thinking = enginePlayer_.thinking();
    snap.activeSide = (game_.sideToMove() == WHITE) ? "White" : "Black";

    if (const auto& info = enginePlayer_.lastInfo())
    {
        snap.depth = info->depth;
        snap.seldepth = info->seldepth;
        snap.nodes = info->nodes;
        snap.nps = info->nps;

        const bool fromBlack = (enginePlayer_.lastInfoSide() == BLACK);
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
    game_.reset();
    clock_.reset();
    clock_.start();
    lastCursor_ = 0;
    gameOverWas_ = false;
    animation_.active = false;
    enginePlayer_.abort();
    controller_.updateAnnotations();
    controller_.reopenGameOverModal();

    // Re-snapshot the initial clock state for the fresh game.
    game_.setInitialClockSnapshot(clock_.snapshot());
}