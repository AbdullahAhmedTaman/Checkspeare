#include "../include/renderUI.h"
#include "SFML/Graphics/RenderWindow.hpp"
#include "Theme.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

namespace RenderUI
{

    sf::RenderWindow initWindow()
    {
        sf::RenderWindow window(sf::VideoMode({1000, 700}), "Chess GUI - SFML + ImGui Test");

        window.setFramerateLimit(60);

        // Initialize ImGui-SFML
        if (!ImGui::SFML::Init(window))
        {
            throw std::runtime_error("[ImGui] Initialization failed!");
        }
        return window;
    }

    void imGuiRenderUI(sf::RenderWindow& window, Config& conf, BoardRenderer::BoardGeometry& geo)
    {
        ImGui::Begin("Chess Engine Test");

        ImGui::Text("SFML + ImGui are working!");

        ImGui::Separator();

        ImGui::Text("Window size: %u x %u", window.getSize().x, window.getSize().y);

        ImGui::SliderFloat("Square Size", &geo.squareSize, 20.0f, 150.0f);

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

    void windowRenderUI(sf::RenderWindow& window, Config& conf, const AssetManager::AssetManager& assets)
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
            BoardRenderer::BoardGeometry geo;
            RenderUI::imGuiRenderUI(window, conf, geo);
            // ----------------------------------------------------
            // SFML rendering
            // ----------------------------------------------------

            window.clear(sf::Color(255, 255, 255));

            // Draw a simple chessboard
            BoardRenderer::BoardRenderer board;
            Theme::BoardTheme theme = Theme::getBoardTheme("Default", assets);
            board.render(window, geo, theme);
            /*
                sf::Sprite testPawn(assets.getTexture("White_Pawn"));

            float scaleX = conf.squareSize / testPawn.getLocalBounds().size.x;
            float scaleY = conf.squareSize / testPawn.getLocalBounds().size.y;

            testPawn.setScale({scaleX, scaleY});
            testPawn.setPosition({0 * conf.squareSize, 6 * conf.squareSize});
            window.draw(testPawn);

            sf::Sprite testKing(assets.getTexture("Black_King"));
            testKing.setScale({scaleX, scaleY});
            testKing.setPosition({4 * conf.squareSize, 0 * conf.squareSize});
            window.draw(testKing);
            */

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
} // namespace RenderUI

namespace BoardRenderer
{

    void BoardRenderer::render(sf::RenderWindow& window, const BoardGeometry& geo, const Theme::BoardTheme& theme)
    {
        for (int rank = 0; rank < 8; ++rank)
        {
            for (int file = 0; file < 8; ++file)
            {
                const sf::Texture* texture = ((rank + file) % 2 == 0) ? theme.Square_Dark : theme.Square_Light;

                sf::Sprite square(*texture);
                // Position
                float x = geo.boardOrigin.x + file * geo.squareSize;
                float y = geo.boardOrigin.y + rank * geo.squareSize;
                square.setPosition({x, y});
                // Scale
                float scaleX = geo.squareSize / square.getLocalBounds().size.x;

                float scaleY = geo.squareSize / square.getLocalBounds().size.y;

                square.setScale({scaleX, scaleY});

                window.draw(square);
            }
        }
    }

} // namespace BoardRenderer