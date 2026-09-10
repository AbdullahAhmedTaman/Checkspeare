#include "../include/renderUI.h"
#include "../include/board.h"
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

        ImGui::SliderFloat2("Board Position", &geo.boardOrigin.x, 0.0f, 300.0f);

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
        BoardRenderer::BoardGeometry geo;
        BoardRenderer::BoardRenderer boardRenderer;
        Theme::BoardTheme theme = Theme::getBoardTheme("Default", assets);

        Chess::Board board;

        board.setupInitialBoard();

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

            RenderUI::imGuiRenderUI(window, conf, geo);
            // ----------------------------------------------------
            // SFML rendering
            // ----------------------------------------------------

            window.clear(sf::Color(255, 255, 255));

            // Draw a simple chessboard -- new board
            boardRenderer.render(window, geo, theme, board, assets);

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
    void BoardRenderer::render(sf::RenderWindow& window, const BoardGeometry& geo, const Theme::BoardTheme& theme,
                               const Chess::Board& board, const AssetManager::AssetManager& assets)
    {
        // board representation
        for (int rank = 0; rank < 8; ++rank)
        {
            for (int file = 0; file < 8; ++file)
            {
                const sf::Texture* texture = ((rank + file) % 2 == 0) ? theme.Square_Dark : theme.Square_Light;

                if (!texture)
                    continue;

                sf::Sprite square(*texture);

                float x = geo.boardOrigin.x + file * geo.squareSize;
                float y = geo.boardOrigin.y + (7 - rank) * geo.squareSize;

                square.setPosition({x, y});

                float scaleX = geo.squareSize / square.getLocalBounds().size.x;
                float scaleY = geo.squareSize / square.getLocalBounds().size.y;
                square.setScale({scaleX, scaleY});

                window.draw(square);
            }
        }

        // Render pieces on the board

        for (Chess::Square sq = 0; sq < 64; ++sq)
        {
            const auto& pieceOpt = board.getPiece(sq);

            if (pieceOpt.has_value())
            {
                const Chess::Piece& piece = pieceOpt.value();

                std::string pieceKey = AssetManager::AssetManager::getPieceKey(piece);

                const sf::Texture& pieceTexture = assets.getTexture(pieceKey);
                sf::Sprite pieceSprite(pieceTexture);

                int file = sq % 8;
                int rank = sq / 8;

                float squareX = geo.boardOrigin.x + file * geo.squareSize;
                float squareY = geo.boardOrigin.y + (7 - rank) * geo.squareSize;

                float paddingRatio = 0.85f;
                float targetSize = geo.squareSize * paddingRatio;

                float scaleX = targetSize / pieceSprite.getLocalBounds().size.x;
                float scaleY = targetSize / pieceSprite.getLocalBounds().size.y;
                pieceSprite.setScale({scaleX, scaleY});

                float offset = (geo.squareSize - targetSize) / 2.0f;
                pieceSprite.setPosition({squareX + offset, squareY + offset});

                window.draw(pieceSprite);
            }
        }
    }
} // namespace BoardRenderer