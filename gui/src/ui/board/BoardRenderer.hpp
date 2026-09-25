#pragma once

#include "../../game/Board.hpp"
#include "BoardAnnotations.hpp"
#include "../../game/BoardGeometry.hpp"
#include "../../controller/GuiGameController.hpp"
#include "../../game/MoveAnimation.hpp"
#include "../../assets/TextureLibrary.hpp"
#include "../Theme.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

namespace Gui
{

    inline constexpr float PieceSpriteScale = 0.85f;

    inline constexpr float piecePadding(float squareSize)
    {
        return squareSize * (1.0f - PieceSpriteScale) * 0.5f;
    }

    /// Draws squares, static pieces, and (if a drag is active) the dragged
    /// piece on top.
    ///
    /// `target` is any SFML render target (window or RenderTexture). When
    /// the board is drawn offscreen for an ImGui panel, pass the pointer
    /// position in the target's coordinate space as `dragPixelPos` so the
    /// dragged piece follows the mouse.
    /// `coordinateFont` is used for the a1–h8 / 1–8 edge labels. The
    /// caller owns it; the reference must remain valid for the call.
    void renderBoard(sf::RenderTarget& target, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const sf::Font& coordinateFont, const DragState& drag,
                     sf::Vector2f dragPixelPos, const PendingPromotion& promo, const BoardAnnotations& annotations,
                     const MoveAnimation& animation);
} // namespace Gui