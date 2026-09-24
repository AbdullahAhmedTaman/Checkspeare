#include "Application.hpp"

#include "Dockspace.hpp"
#include "EngineThinkingPanel.hpp"
#include "ImGuiLayer.hpp"
#include "ImGuiTheme.hpp"
#include "MoveListPanel.hpp"
#include "SfmlInputAdapter.hpp"

#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

#include <stdexcept>

Application::Application(Config& conf, const Assets::TextureLibrary& assets)
    : conf_(conf),
      assets_(assets),
      window_(sf::VideoMode({1280, 800}), "Chess GUI - SFML + ImGui"),
      controller_(game_),
      theme_(Gui::getBoardTheme("Default", assets))
{
    window_.setFramerateLimit(60);

    // Default time control.
    ChessClock::TimeControl tc;
    tc.initialMs = 60 * 1000;
    tc.incrementMs = 3 * 1000;
    clock_.configure(tc);
    clock_.start();

    if (!ImGui::SFML::Init(window_, false))
        throw std::runtime_error("[ImGui] Initialization failed!");

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
        if (!inputEvent)
            continue;

        // Allow board input and drag termination despite ImGui mouse capture.
        const bool imguiCaptures = ImGui::GetIO().WantCaptureMouse;
        const bool endsDrag = inputEvent->kind == Gui::InputEvent::Kind::PointerReleased && controller_.drag().active;
        const bool focusLost = inputEvent->kind == Gui::InputEvent::Kind::FocusLost;
        const bool engineToMove = enginePlayer_.isSideControlled(game_.sideToMove());

        const bool overBoard = !focusLost && boardPanel_.contains(inputEvent->position);

        const bool shouldForward = focusLost || endsDrag || (!engineToMove && (overBoard || !imguiCaptures));

        if (!shouldForward)
            continue;

        Gui::InputEvent forwarded = *inputEvent;
        if (!focusLost)
            forwarded.position = boardPanel_.toBoardSpace(forwarded.position);

        controller_.handleEvent(forwarded, geo_);
    }
}

void Application::update(float dtSeconds)
{
    controller_.applyQueuedActions();
    enginePlayer_.tick(game_, clock_);
    controller_.updateAnnotations();
    Gui::EngineThinkingSnapshot snap;
    snap.thinking = enginePlayer_.thinking();
    snap.activeSide = (game_.sideToMove() == WHITE) ? "White" : "Black";

    if (const auto& info = enginePlayer_.lastInfo())
    {
        snap.depth = info->depth;
        snap.seldepth = info->seldepth;
        snap.nodes = info->nodes;
        snap.nps = info->nps;

        // Normalize the score to White's perspective.
        const bool fromBlack = (enginePlayer_.lastInfoSide() == BLACK);

        if (info->scoreCp)
            snap.scoreCp = fromBlack ? -(*info->scoreCp) : *info->scoreCp;
        if (info->scoreMate)
            snap.scoreMate = fromBlack ? -(*info->scoreMate) : *info->scoreMate;
    }

    if (snap.scoreMate)
        evalBar_.setTarget(0, *snap.scoreMate);
    else if (snap.scoreCp)
        evalBar_.setTarget(*snap.scoreCp, 0);
    evalBar_.update(dtSeconds);

    ImGui::SFML::Update(window_, sf::seconds(dtSeconds));

    Gui::beginMainDockspace();
    boardPanel_.render(geo_, theme_, game_.board(), assets_, coordinateFont_, controller_.drag(), controller_.pendingPromotion(),
                       controller_.annotations());
    Gui::renderEngineThinkingPanel(snap, evalBar_);
    Gui::renderImGui(window_, conf_, geo_, controller_, game_, clock_, clockFont_);
    Gui::renderMoveListPanel(game_, controller_);

    // Advance the clock for each newly committed ply.
    const std::size_t curr = game_.cursor();
    while (lastCursor_ < curr)
    {
        const Color mover = (lastCursor_ % 2 == 0) ? WHITE : BLACK;
        clock_.onMovePlayed(mover);
        ++lastCursor_;
    }

    // Undo and history navigation do not rewind elapsed time.
    if (curr < lastCursor_)
        lastCursor_ = curr;
    clock_.update(dtSeconds);
}

void Application::render()
{
    window_.clear(sf::Color::White);
    ImGui::SFML::Render(window_);
    window_.display();
}