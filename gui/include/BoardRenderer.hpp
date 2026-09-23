#pragma once

#include "Board.hpp"
#include "BoardAnnotations.hpp"
#include "BoardGeometry.hpp"
#include "GuiGameController.hpp"
#include "TextureLibrary.hpp"
#include "Theme.hpp"


#include <SFML/Graphics/RenderWindow.hpp>

namespace Gui
{

    inline constexpr float PieceSpriteScale = 0.85f;

    inline constexpr float piecePadding(float squareSize)
    {
        return squareSize * (1.0f - PieceSpriteScale) * 0.5f;
    }

    /// Draws squares, static pieces, and (if a drag is active) the dragged
    /// piece on top.
    void renderBoard(sf::RenderWindow& window, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const DragState& drag, const PendingPromotion& promo,
                     const BoardAnnotations& annotations);

} // namespace Gui