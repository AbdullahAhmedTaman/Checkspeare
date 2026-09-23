#include "Application.hpp"

#include "BoardRenderer.hpp"
#include "ImGuiLayer.hpp"
#include "SfmlInputAdapter.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <SFML/Window/Event.hpp>

#include <stdexcept>

Application::Application(Config& conf, const Assets::TextureLibrary& assets)
    : conf_(conf),
      assets_(assets),
      window_(sf::VideoMode({1000, 700}), "Chess GUI - SFML + ImGui"),
      controller_(game_),
      theme_(Gui::getBoardTheme("Default", assets))
{
    window_.setFramerateLimit(60);

    // Default time control: 5 minutes + 3 seconds increment per move.
    // Adjust here or, later, read from Config.
    ChessClock::TimeControl tc;
    tc.initialMs = 5 * 60 * 1000;
    tc.incrementMs = 3 * 1000;
    clock_.configure(tc);
    clock_.start();

    if (!ImGui::SFML::Init(window_))
        throw std::runtime_error("[ImGui] Initialization failed!");
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

        // Do not forward to the controller while ImGui owns the mouse, except
        // for a release that ends an in-progress drag, otherwise the drag
        // state can get stuck when the user releases over an ImGui panel.
        const bool imguiCaptures = ImGui::GetIO().WantCaptureMouse;
        const bool endsDrag = event->is<sf::Event::MouseButtonReleased>() && controller_.drag().active;
        const bool focusLost = event->is<sf::Event::FocusLost>();
        const bool pickerOpen = controller_.pendingPromotion().active;

        if (!imguiCaptures || endsDrag || focusLost || pickerOpen)
        {
            if (const auto inputEvent = Gui::toInputEvent(*event))
                controller_.handleEvent(*inputEvent, geo_);
        }
    }
}

void Application::update(float dtSeconds)
{
    ImGui::SFML::Update(window_, sf::seconds(dtSeconds));

    // Detect cursor advancement since the last frame. Each new ply means a
    // move was committed: notify the clock so it can add the mover's
    // increment and flip to the other side.
    //
    // The mover of ply `i` (0-based) is White when i is even, Black when
    // odd. That holds for any game that starts from the standard position
    // with White to move. A FEN-loaded game where Black is to move would
    // require resetting lastCursor_ and reconfiguring the clock; that
    // feature isn't wired yet.
    const std::size_t curr = game_.cursor();
    while (lastCursor_ < curr)
    {
        const Color mover = (lastCursor_ % 2 == 0) ? WHITE : BLACK;
        clock_.onMovePlayed(mover);
        ++lastCursor_;
    }

    // If the cursor moved backwards (undo, or a jump in the history),
    // resync lastCursor_ but do NOT rewind the clock. Time already spent
    // is gone — that's the standard MVP compromise.
    if (curr < lastCursor_)
        lastCursor_ = curr;

    clock_.update(dtSeconds);

    Gui::renderImGui(window_, conf_, geo_, controller_, game_, clock_);
    controller_.applyQueuedActions();
}

void Application::render()
{
    window_.clear(sf::Color::White);
    Gui::renderBoard(window_, geo_, theme_, game_.board(), assets_, controller_.drag(), controller_.pendingPromotion());
    ImGui::SFML::Render(window_);
    window_.display();
}