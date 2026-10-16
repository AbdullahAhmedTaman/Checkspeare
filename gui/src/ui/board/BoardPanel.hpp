#pragma once

#include "game/Board.hpp"
#include "BoardAnnotations.hpp"
#include "game/BoardGeometry.hpp"
#include "controller/GuiGameController.hpp"
#include "game/MoveAnimation.hpp"
#include "assets/TextureLibrary.hpp"
#include "ui/Theme.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/System/Vector2.hpp>

namespace Gui
{

    /// The dockable "Board" panel.
    ///
    /// Owns the offscreen RenderTexture the board is drawn into and the
    /// on-screen rectangle that texture occupies. The board is not drawn
    /// directly to the window: it is rendered to a texture and blitted with
    /// ImGui::Image, which lets the board participate in the dock layout.
    ///
    /// The panel is also the translation boundary between window-space
    /// pointer coordinates (what SFML and ImGui report) and board-space
    /// coordinates (what BoardGeometry and the controller expect).
    class BoardPanel
    {
    public:
        /// Renders the board for this frame and updates `geo` to describe
        /// the panel's board area (origin at 0,0, squareSize fit to the
        /// panel). The flipped flag on `geo` is left untouched.
        /// `coordinateFont` is used to draw the a1–h8 / 1–8 labels along
        /// the board edges. The caller owns it and must keep it alive.
        /// `animation` describes a piece in flight; while active, the renderer
        /// draws it interpolated and skips the static piece at its destination.
        void render(BoardGeometry& geo, const BoardTheme& theme, const Board& board, const Assets::TextureLibrary& assets,
                    const sf::Font& coordinateFont, const DragState& drag, const PendingPromotion& promo,
                    const BoardAnnotations& annotations, const MoveAnimation& animation, const std::vector<Arrow>& arrows);

        /// True if `windowPos` (window coordinates) falls inside the board
        /// image as drawn this frame. Used to bypass ImGui's mouse capture
        /// for the board's own panel.
        bool contains(sf::Vector2f windowPos) const;

        /// Translates a window-space pointer position into board-space
        /// coordinates. Safe to call regardless of `contains`.
        sf::Vector2f toBoardSpace(sf::Vector2f windowPos) const;

    private:
        sf::RenderTexture texture_;
        sf::Vector2f origin_{0.0f, 0.0f};
        sf::Vector2f size_{0.0f, 0.0f};
    };

} // namespace Gui