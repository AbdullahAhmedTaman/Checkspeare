#include "renderUI.h"
#include "SFML/Graphics/RenderWindow.hpp"
#include "Theme.h"
#include "board.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <imgui-SFML.h>
#include <imgui.h>


namespace RenderUI
{
    using std::string;

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

    void imGuiRenderUI(sf::RenderWindow& window, Config& conf, BoardRenderer::BoardGeometry& geo, const std::string& clickedInfo)
    {
        ImGui::Begin("Chess Engine Test");

        ImGui::Text("SFML + ImGui are working!");

        ImGui::Separator();

        ImGui::Text("Window size: %u x %u", window.getSize().x, window.getSize().y);

        ImGui::SliderFloat("Square Size", &geo.squareSize, 20.0f, 150.0f);

        ImGui::SliderFloat2("Board Position", &geo.boardOrigin.x, 0.0f, 300.0f);

        ImGui::Separator();

        ImGui::Text("Last Clicked Square:");

        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", clickedInfo.c_str());

        ImGui::Separator();

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

    void windowRenderUI(sf::RenderWindow& window, Config& conf, const Assets::AssetManager& assets)
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
        std::string clickedInfo = "None (Click anywhere!)";

        bool isDragging = false;
        Chess::Square draggedSquare = 0;
        sf::Vector2f dragOffset(0.0f, 0.0f);

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
                if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mousePress->button == sf::Mouse::Button::Left)
                    {
                        float relativeX = static_cast<float>(mousePress->position.x) - geo.boardOrigin.x;
                        float relativeY = static_cast<float>(mousePress->position.y) - geo.boardOrigin.y;

                        float boardWidth = geo.squareSize * 8.0f;
                        float boardHeight = geo.squareSize * 8.0f;

                        if (relativeX >= 0.0f && relativeX < boardWidth && relativeY >= 0.0f && relativeY < boardHeight)
                        {
                            int file = static_cast<int>(relativeX / geo.squareSize);
                            int rank = 7 - static_cast<int>(relativeY / geo.squareSize);

                            Chess::Square sq = static_cast<Chess::Square>(rank * 8 + file);

                            std::string uci = Chess::Board::SquareToUci(sq);
                            std::string sqColor = Chess::Board::getSquareColor(sq);

                            const auto& pieceOpt = board.getPiece(sq);
                            if (pieceOpt.has_value())
                            {
                                isDragging = true;
                                draggedSquare = sq;

                                float squareX = geo.boardOrigin.x + file * geo.squareSize;
                                float squareY = geo.boardOrigin.y + (7 - rank) * geo.squareSize;
                                dragOffset = sf::Vector2f(mousePress->position.x - squareX, mousePress->position.y - squareY);

                                std::string uci = Chess::Board::SquareToUci(sq);
                                std::string sqColor = Chess::Board::getSquareColor(sq);
                                clickedInfo = "Dragging " + uci + " (" + sqColor + ")";
                            }
                        }
                    }
                }
                if (const auto* mouseRelease = event->getIf<sf::Event::MouseButtonReleased>())
                {
                    if (mouseRelease->button == sf::Mouse::Button::Left && isDragging)
                    {
                        float relativeX = static_cast<float>(mouseRelease->position.x) - geo.boardOrigin.x;
                        float relativeY = static_cast<float>(mouseRelease->position.y) - geo.boardOrigin.y;

                        float boardWidth = geo.squareSize * 8.0f;
                        float boardHeight = geo.squareSize * 8.0f;

                        if (relativeX >= 0.0f && relativeX < boardWidth && relativeY >= 0.0f && relativeY < boardHeight)
                        {
                            int file = static_cast<int>(relativeX / geo.squareSize);
                            int rank = 7 - static_cast<int>(relativeY / geo.squareSize);
                            Chess::Square targetSq = static_cast<Chess::Square>(rank * 8 + file);

                            const auto& movingPiece = board.getPiece(draggedSquare);
                            board.setPiece(targetSq, movingPiece);
                            board.setPiece(draggedSquare, std::nullopt);
                        }
                        else
                        {
                            clickedInfo = "Dropped Outside Board";
                        }

                        isDragging = false;
                    }
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

            RenderUI::imGuiRenderUI(window, conf, geo, clickedInfo);
            // ----------------------------------------------------
            // SFML rendering
            // ----------------------------------------------------

            window.clear(sf::Color(255, 255, 255));

            // Draw a simple chessboard -- new board
            boardRenderer.render(window, geo, theme, board, assets, isDragging, draggedSquare, dragOffset);
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

// namespace RenderUI

namespace BoardRenderer
{
    void BoardRenderer::render(sf::RenderWindow& window, const BoardGeometry& geo, const Theme::BoardTheme& theme,
                               const Chess::Board& board, const Assets::AssetManager& assets, bool isDragging,
                               Chess::Square draggedSquare, sf::Vector2f dragOffset)
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

        for (Chess::Square sq = 0; sq < 64; ++sq)
        {
            if (isDragging && sq == draggedSquare)
                continue;
            const auto& pieceOpt = board.getPiece(sq);
            if (pieceOpt.has_value())
            {
                const Chess::Piece& piece = pieceOpt.value();
                std::string pieceKey = Assets::AssetManager::getPieceKey(piece);
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
        if (isDragging)
        {
            const auto& pieceOpt = board.getPiece(draggedSquare);
            if (pieceOpt.has_value())
            {
                const Chess::Piece& piece = pieceOpt.value();
                std::string pieceKey = Assets::AssetManager::getPieceKey(piece);
                const sf::Texture& pieceTexture = assets.getTexture(pieceKey);
                sf::Sprite draggedSprite(pieceTexture);

                float paddingRatio = 0.85f;
                float targetSize = geo.squareSize * paddingRatio;

                float scaleX = targetSize / draggedSprite.getLocalBounds().size.x;
                float scaleY = targetSize / draggedSprite.getLocalBounds().size.y;
                draggedSprite.setScale({scaleX, scaleY});

                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                float drawX = static_cast<float>(mousePos.x) - dragOffset.x;
                float drawY = static_cast<float>(mousePos.y) - dragOffset.y;

                draggedSprite.setPosition({drawX, drawY});
                window.draw(draggedSprite);
            }
        }
    }
} // namespace BoardRenderer

// namespace BoardRenderer