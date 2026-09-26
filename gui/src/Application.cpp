#include "Application.hpp"

#include "game/GameOverTransition.hpp"
#include "game/GameResult.hpp"
#include "input/InputEvent.hpp"
#include "input/SfmlInputAdapter.hpp"
#include "ui/ClockPanel.hpp"
#include "ui/DockSpace.hpp"
#include "ui/ImGuiTheme.hpp"
#include "ui/MoveListPanel.hpp"
#include "ui/StatusPanel.hpp"
#include "ui/dialogs/GameOverModal.hpp"
#include "ui/engine/EngineThinkingPanel.hpp"


#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <stdexcept>

namespace
{
    const char* soundKeyFor(MoveSound s)
    {
        switch (s)
        {
            case MoveSound::QuietMove:
                return Assets::SoundKey::Move;
            case MoveSound::Capture:
                return Assets::SoundKey::Capture;
            case MoveSound::Check:
                return Assets::SoundKey::Check;
            case MoveSound::Castle:
                return Assets::SoundKey::Castle;
            case MoveSound::Promotion:
                return Assets::SoundKey::Promote;
        }
        return Assets::SoundKey::Move;
    }
} // namespace

Application::Application(Config& conf, const Assets::TextureLibrary& assets, const Assets::SoundLibrary& sounds)
    : conf_(conf),
      assets_(assets),
      sounds_(sounds),
      window_(sf::VideoMode({1280, 800}), "Chess GUI"),
      controller_(game_),
      theme_(Gui::getBoardTheme("Default", assets)),
      soundPlayer_(sounds)
{
    window_.setFramerateLimit(60);

    // Clock is configured but not started. It stays stopped until a setup
    // is committed, so no time is lost while the user reads the modal.
    clock_.configure(conf_.timeControl);
    clock_.stop();

    wireClockCallbacks();

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

    seedSetupModalFromConfig();
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
            soundPlayer_.play(soundKeyFor(game_.history()[ply].sound));
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

void Application::seedSetupModalFromConfig()
{
    // Committed values are written back into Config by applyGameSetup, so
    // this seed gives the next session the last-used settings.
    pendingSetup_.timeControl = conf_.timeControl;

    // Guard against a stored zero from an earlier session: a clock with
    // zero time falls on the first tick, before either side can move.
    if (pendingSetup_.timeControl.initialMs < 60 * 1000)
        pendingSetup_.timeControl.initialMs = 60 * 1000;

    pendingSetup_.whiteKind = Gui::PlayerKind::Human;
    pendingSetup_.blackKind = Gui::PlayerKind::Engine;
    pendingSetup_.whiteEnginePath = conf_.whiteEnginePath;
    pendingSetup_.blackEnginePath = conf_.blackEnginePath;
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
        // Runs before shouldForwardInput because ImGui-captured panel
        // clicks are otherwise not forwarded. Skipped while a modal is
        // open: the modal owns every click.
        if (!setupModalOpen_ && !isGameOver() && inputEvent->kind == Gui::InputEvent::Kind::PointerPressed &&
            inputEvent->button == Gui::MouseButton::Left && !boardPanel_.contains(inputEvent->position))
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

    // Modals gate everything else. ImGui's WantCaptureMouse alone is not
    // enough: the board is itself an ImGui panel, so a click over its
    // rectangle would otherwise be forwarded regardless of what is on top.
    if (setupModalOpen_ || isGameOver())
        return false;

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

bool Application::canResign() const
{
    if (isGameOver())
        return false;
    return !sideIsEngineControlled(game_.sideToMove());
}

bool Application::sideIsHuman(Color side) const
{
    const bool whitePlaysIt = playerWhite_.isSideControlled(side);
    const bool blackPlaysIt = playerBlack_.isSideControlled(side);
    return !whitePlaysIt && !blackPlaysIt;
}

std::string Application::gameOverSoundKey() const
{
    // Time loss beats anything the board says: the flag decides the game.
    const auto outcomeFor = [this](Color loser)
    {
        const Color winner = (loser == WHITE) ? BLACK : WHITE;
        const bool loserHuman = sideIsHuman(loser);
        const bool winnerHuman = sideIsHuman(winner);

        if (loserHuman && !winnerHuman)
            return std::string(Assets::SoundKey::Defeat);
        if (winnerHuman && !loserHuman)
            return std::string(Assets::SoundKey::Victory);

        // Both sides human (hotseat) or both engines (spectating): play
        // the victory cue for whoever won.
        return std::string(Assets::SoundKey::Victory);
    };

    if (clock_.flagFallen(WHITE))
        return outcomeFor(WHITE);
    if (clock_.flagFallen(BLACK))
        return outcomeFor(BLACK);

    const GameResult result = game_.result();

    switch (result.kind)
    {
        case GameResultKind::Checkmate:
        case GameResultKind::Resignation:
            {
                const Color loser = (result.winner == WHITE) ? BLACK : WHITE;
                return outcomeFor(loser);
            }

        case GameResultKind::Stalemate:
        case GameResultKind::FiftyMove:
        case GameResultKind::Threefold:
        case GameResultKind::InsufficientMaterial:
            return std::string(Assets::SoundKey::Draw);

        case GameResultKind::Ongoing:
            return {};
    }
    return {};
}

bool Application::sideIsEngineControlled(Color side) const
{
    // A side is blocked from human input only when an engine is actually
    // running and configured to play it. Configuration alone is a
    // preference; starting an engine is what makes it take effect.
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

    if (auto resigner = controller_.consumeResignRequest())
        game_.resign(*resigner);

    controller_.applyQueuedActions();

    // Input-feedback sounds. These outcomes have no MoveRecord counterpart
    // (a rejected or abandoned move is never committed), so they are keyed
    // off the controller's ActionResult instead. The kind comparison against
    // the previous frame prevents the sound from re-firing every frame the
    // outcome remains current.
    const Gui::ActionResult::Kind currentKind = controller_.result().kind;
    if (currentKind != lastResultKind_)
    {
        switch (currentKind)
        {
            case Gui::ActionResult::Kind::IllegalMove:
                soundPlayer_.play(Assets::SoundKey::Illegal);
                break;
            default:
                break;
        }
        lastResultKind_ = currentKind;
    }

    const bool setupOpen = setupModalOpen_;

    if (!setupOpen)
    {
        playerWhite_.tick(game_, clock_);
        playerBlack_.tick(game_, clock_);
    }

    controller_.updateAnnotations();

    const bool gameOver = isGameOver();
    const std::size_t curr = game_.cursor();

    // Animation: a single forward step is a real commit; a single backward
    // step is an undo. Multi-ply jumps snap.
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

    // Cursor bookkeeping.
    //
    // The clock is advanced only by moveCommitted_, which fires from
    // ChessGame::play. Every other cursor change already pushed the
    // matching snapshot through clockSync_, which *restores* it. Treating
    // a redo as a commit here would re-apply increments on top of the
    // restored snapshot and flip the active side twice.
    lastCursor_ = curr;

    if (gameOver && !gameOverWas_)
    {
        playerWhite_.abort();
        playerBlack_.abort();
        controller_.cancelSelection();
        controller_.reopenGameOverModal();

        const std::string outcome = gameOverSoundKey();
        if (!outcome.empty())
            soundPlayer_.play(outcome);
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
    Gui::renderStatusPanel(controller_, game_, canResign());
    Gui::renderGameOverModal(controller_, game_, clock_, setupModalOpen_);

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

    Gui::renderClockPanel(clock_, game_, clockFont_, engineNameFont_, displayName(engineWhite_), displayName(engineBlack_));
    Gui::renderMoveListPanel(game_, controller_, moveListFont_);

    if (auto committed = Gui::renderGameSetupModal(pendingSetup_, setupModalOpen_))
    {
        applyGameSetup(*committed);
        setupModalOpen_ = false;
    }
}

Gui::EngineThinkingSnapshot Application::buildThinkingSnapshot() const
{
    Gui::EngineThinkingSnapshot snap;

    const bool whiteActive = engineWhite_.isRunning() && playerWhite_.isSideControlled(WHITE);
    const bool blackActive = engineBlack_.isRunning() && playerBlack_.isSideControlled(BLACK);

    const EnginePlayer* active = nullptr;
    const UciEngine* activeEngine = nullptr;

    if (whiteActive && blackActive)
    {
        if (game_.sideToMove() == WHITE)
        {
            active = &playerWhite_;
            activeEngine = &engineWhite_;
        }
        else
        {
            active = &playerBlack_;
            activeEngine = &engineBlack_;
        }
    }
    else if (whiteActive)
    {
        active = &playerWhite_;
        activeEngine = &engineWhite_;
    }
    else if (blackActive)
    {
        active = &playerBlack_;
        activeEngine = &engineBlack_;
    }

    if (active == nullptr)
        return snap;

    snap.activeSide = (active == &playerWhite_) ? "White" : "Black";
    snap.engineName = activeEngine->name();
    snap.thinking = active->thinking();

    if (const auto& info = active->lastInfo())
    {
        snap.depth = info->depth;
        snap.seldepth = info->seldepth;
        snap.nodes = info->nodes;
        snap.nps = info->nps;

        // UCI reports from the engine's own point of view; normalize to
        // White's perspective for display.
        const bool fromBlack = (active->lastInfoSide() == BLACK);
        if (info->scoreCp)
            snap.scoreCp = fromBlack ? -(*info->scoreCp) : *info->scoreCp;
        if (info->scoreMate)
            snap.scoreMate = fromBlack ? -(*info->scoreMate) : *info->scoreMate;
    }

    return snap;
}

void Application::newGame()
{
    // Defer the reset to applyGameSetup. The modal needs the old game to
    // still be alive while it is on screen, and nothing changes until the
    // user commits a fresh setup.
    setupModalOpen_ = true;
    controller_.updateAnnotations();
}

void Application::applyGameSetup(const Gui::GameSetup& setup)
{
    conf_.timeControl = setup.timeControl;
    conf_.whiteEnginePath = setup.whiteEnginePath;
    conf_.blackEnginePath = setup.blackEnginePath;

    playerWhite_.abort();
    playerBlack_.abort();
    engineWhite_.stop();
    engineBlack_.stop();

    game_.reset();
    lastCursor_ = 0;
    gameOverWas_ = false;
    animation_.active = false;
    suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();

    // Clamp the initial time to one minute: zero would make the flag fall
    // on the first tick, before either side can move.
    ChessClock::TimeControl tc = setup.timeControl;
    if (tc.initialMs < 60 * 1000)
        tc.initialMs = 60 * 1000;

    clock_.configure(tc);
    clock_.start();
    game_.setInitialClockSnapshot(clock_.snapshot());

    playerWhite_.setSideControlled(WHITE, setup.whiteKind == Gui::PlayerKind::Engine);
    playerBlack_.setSideControlled(BLACK, setup.blackKind == Gui::PlayerKind::Engine);

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
        // A bad path or failed spawn leaves the engine stopped. The setup
        // panel can retry without restarting the GUI.
    }
}