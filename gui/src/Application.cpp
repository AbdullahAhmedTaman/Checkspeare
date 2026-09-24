#include "Application.hpp"

#include "Dockspace.hpp"
#include "EngineThinkingPanel.hpp"
#include "ImGuiLayer.hpp"
#include "ImGuiTheme.hpp"
#include "InputEvent.hpp"
#include "MoveListPanel.hpp"
#include "SfmlInputAdapter.hpp"

#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

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

    if (!ImGui::SFML::Init(window_, false))
        throw std::runtime_error("[ImGui] Initialization failed!");

    // Layout is rebuilt by DockBuilder every launch; disable persistence
    // so the on-disk state can't drift from the code's intent.
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
    // FocusLost and drag-ending releases always forward: the drag state
    // machine must see them regardless of who owns the mouse.
    if (ev.kind == Gui::InputEvent::Kind::FocusLost)
        return true;

    const bool endsDrag = ev.kind == Gui::InputEvent::Kind::PointerReleased && controller_.drag().active;
    if (endsDrag)
        return true;

    // Never let the human play for an engine-controlled side.
    if (enginePlayer_.isSideControlled(game_.sideToMove()))
        return false;

    // Board controls bypass ImGui capture when the pointer is over the panel.
    if (boardPanel_.contains(ev.position))
        return true;

    return !ImGui::GetIO().WantCaptureMouse;
}

void Application::update(float dtSeconds)
{
    stepSimulation(dtSeconds);
    stepUi(dtSeconds);
}

void Application::stepSimulation(float dtSeconds)
{
    controller_.applyQueuedActions();
    enginePlayer_.tick(game_, clock_);
    controller_.updateAnnotations();

    const std::size_t curr = game_.cursor();

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
        // Undo. history()[curr] is the move that was just un-applied: its
        // from/to are swapped so the piece slides back to its origin, and
        // the piece drawn is whatever sits on that origin now -- a pawn if
        // the un-done move was a promotion.
        const MoveRecord& rec = game_.history()[curr];
        animation_.start(rec.move.to(), rec.move.from(), game_.pieceAt(rec.move.from()));
    }
    else if (curr != lastCursor_)
    {
        // gotoPly, gotoEnd, loadFEN: multi-ply jumps snap.
        animation_.active = false;
    }

    suppressAnimationAtPly_ = std::numeric_limits<std::size_t>::max();

    animation_.tick(dtSeconds);

    while (lastCursor_ < curr)
    {
        const Color mover = (lastCursor_ % 2 == 0) ? WHITE : BLACK;
        clock_.onMovePlayed(mover);
        ++lastCursor_;
    }
    if (curr < lastCursor_)
        lastCursor_ = curr;

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

        // Normalize to White's perspective for display.
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