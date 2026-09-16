#include "Application.hpp"

#include "BoardRenderer.hpp"
#include "ImGuiLayer.hpp"
#include "SfmlInputAdapter.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <SFML/Window/Event.hpp>

#include <stdexcept>

Application::Application(Config& conf, const Assets::AssetManager& assets)
    : conf_(conf),
      assets_(assets),
      window_(sf::VideoMode({1000, 700}), "Chess GUI - SFML + ImGui"),
      controller_(game_), // was controller_(engine_)
      theme_(Gui::getBoardTheme("Default", assets))
{
    window_.setFramerateLimit(60);

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

        // Do not forward events to the controller while ImGui owns the mouse,
        // except for a release that ends an in-progress drag. Otherwise the
        // drag state can get stuck if the user releases over an ImGui panel.
        const bool imguiCaptures = ImGui::GetIO().WantCaptureMouse;
        const bool endsDrag = event->is<sf::Event::MouseButtonReleased>() && controller_.drag().active;
        const bool focusLost = event->is<sf::Event::FocusLost>();

        if (!imguiCaptures || endsDrag || focusLost)
        {
            if (const auto inputEvent = Gui::toInputEvent(*event))
                controller_.handleEvent(*inputEvent, geo_);
        }
    }
}

void Application::update(float dtSeconds)
{
    ImGui::SFML::Update(window_, sf::seconds(dtSeconds));
    Gui::renderImGui(window_, conf_, geo_, controller_);
}

void Application::render()
{
    window_.clear(sf::Color::White);
    Gui::renderBoard(window_, geo_, theme_, game_, assets_, controller_.drag());
    ImGui::SFML::Render(window_);
    window_.display();
}