#pragma once

#include "types.h"

#include <SFML/System/Vector2.hpp>

#include <optional>

namespace Gui
{

    /// Maps between board squares and pixel coordinates.
    /// `flipped == true` places Black at the bottom of the view.
    struct BoardGeometry
    {
        float squareSize = 80.0f;
        sf::Vector2f boardOrigin{40.0f, 40.0f};
        bool flipped = false;

        float boardWidth() const
        {
            return squareSize * 8.0f;
        }

        float boardHeight() const
        {
            return squareSize * 8.0f;
        }

        bool contains(sf::Vector2f p) const
        {
            const float dx = p.x - boardOrigin.x;
            const float dy = p.y - boardOrigin.y;
            return dx >= 0.0f && dy >= 0.0f && dx < boardWidth() && dy < boardHeight();
        }

        static void squareToGrid(Square sq, bool flipped, int& gridX, int& gridY)
        {
            const int file = static_cast<int>(sq) % 8;
            const int rank = static_cast<int>(sq) / 8;
            gridX = flipped ? 7 - file : file;
            gridY = flipped ? rank : 7 - rank;
        }

        static Square gridToSquare(int gridX, int gridY, bool flipped)
        {
            if (gridX < 0 || gridX > 7 || gridY < 0 || gridY > 7)
                return SQ_NONE;
            if (flipped)
            {
                gridX = 7 - gridX;
                gridY = 7 - gridY;
            }
            return static_cast<Square>((7 - gridY) * 8 + gridX);
        }

        sf::Vector2f topLeft(Square sq) const
        {
            int gx, gy;
            squareToGrid(sq, flipped, gx, gy);
            return {boardOrigin.x + gx * squareSize, boardOrigin.y + gy * squareSize};
        }

        std::optional<Square> squareAt(sf::Vector2f p) const
        {
            if (!contains(p))
                return std::nullopt;
            const int gx = static_cast<int>((p.x - boardOrigin.x) / squareSize);
            const int gy = static_cast<int>((p.y - boardOrigin.y) / squareSize);
            return gridToSquare(gx, gy, flipped);
        }
    };

} // namespace Gui