#include "renderUI.h"
#include "SFML/Graphics/RenderWindow.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

namespace renderUI
{

    sf::RenderWindow initWindow()
    {
        sf::RenderWindow window(sf::VideoMode({1000, 700}), "Chess GUI - SFML + ImGui Test");

        window.setFramerateLimit(60);

        // Initialize ImGui-SFML
        if (!ImGui::SFML::Init(window))
        {
            throw std::runtime_error("ImGui initialization failed!");
        }
        return window;
    }

    void imGuiRenderUI(sf::RenderWindow& window, Config& conf)
    {
        ImGui::Begin("Chess Engine Test");

        ImGui::Text("SFML + ImGui are working!");

        ImGui::Separator();

        ImGui::Text("Window size: %u x %u", window.getSize().x, window.getSize().y);

        ImGui::SliderFloat("Square Size", &conf.squareSize, 20.0f, 150.0f);

        if (ImGui::Button("Test Button"))
        {
            // Just testing interaction
        }

        ImGui::Checkbox("Show ImGui Demo", &conf.showDemoWindow);

        ImGui::End();

        if (conf.showDemoWindow)
        {
            ImGui::ShowDemoWindow(&conf.showDemoWindow);
        }
    }

    void windowRenderUI(sf::RenderWindow& window, Config& conf)
    {
        sf::Clock deltaClock;
        // ---
        // main Render Loop
        // ---
        while (window.isOpen())
        {
            // ----------------------------------------------------
            // Events
            // ----------------------------------------------------

            while (const std::optional event = window.pollEvent())
            {
                ImGui::SFML::ProcessEvent(window, *event);

                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }
            }

            // ----------------------------------------------------
            // Update ImGui
            // ----------------------------------------------------

            sf::Time deltaTime = deltaClock.restart();

            ImGui::SFML::Update(window, deltaTime);

            // ----------------------------------------------------
            // ImGui Components
            // ----------------------------------------------------
            renderUI::imGuiRenderUI(window, conf);
            // ----------------------------------------------------
            // SFML rendering
            // ----------------------------------------------------

            window.clear(sf::Color(30, 30, 30));

            // Draw a simple chessboard
            for (int rank = 0; rank < 8; ++rank)
            {
                for (int file = 0; file < 8; ++file)
                {
                    sf::RectangleShape square(sf::Vector2f(conf.squareSize, conf.squareSize));

                    square.setPosition({file * conf.squareSize, rank * conf.squareSize});

                    if ((rank + file) % 2 == 0)
                    {
                        square.setFillColor(sf::Color(240, 217, 181));
                    }
                    else
                    {
                        square.setFillColor(sf::Color(181, 136, 99));
                    }

                    window.draw(square);
                }
            }

            // ----------------------------------------------------
            // ImGui rendering
            // ----------------------------------------------------

            ImGui::SFML::Render(window);

            window.display();
        }

        // --------------------------------------------------------
        // Shutdown
        // --------------------------------------------------------

        ImGui::SFML::Shutdown();
    }
} // namespace renderUI