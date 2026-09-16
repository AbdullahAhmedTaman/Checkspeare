#pragma once

#include "AssetsManager.h"
#include "BoardGeometry.hpp"
#include "ChessGame.hpp"
#include "GuiGameController.hpp"
#include "Theme.h"

#include <SFML/Graphics/RenderWindow.hpp>

namespace Gui
{

    inline constexpr float PieceSpriteScale = 0.85f;

    inline constexpr float piecePadding(float squareSize)
    {
        return squareSize * (1.0f - PieceSpriteScale) * 0.5f;
    }

    /// Renders the board, static pieces, and (if a drag is in progress) the
    /// dragged piece on top.
    ///
    /// Takes the drag state directly rather than the controller so that the
    /// draw path has no dependency on the input layer.
    void renderBoard(sf::RenderWindow& window, const BoardGeometry& geo, const BoardTheme& theme, const ChessGame& game,
                     const Assets::AssetManager& assets, const DragState& drag);

} // namespace Gui