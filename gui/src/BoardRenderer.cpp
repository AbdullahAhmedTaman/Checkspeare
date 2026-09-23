#include "BoardRenderer.hpp"
#include "BoardAnnotations.hpp"
#include "TextureLibrary.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Mouse.hpp>

namespace
{
    sf::Sprite makePieceSprite(const Assets::TextureLibrary& assets, Piece pc, const Gui::BoardGeometry& geo)
    {
        const sf::Texture& tex = assets.getTexture(Assets::TextureLibrary::getPieceKey(pc));
        sf::Sprite sprite(tex);

        const float targetSize = geo.squareSize * Gui::PieceSpriteScale;
        const auto bounds = sprite.getLocalBounds();
        sprite.setScale({targetSize / bounds.size.x, targetSize / bounds.size.y});
        return sprite;
    }

    void drawPromotionPicker(sf::RenderWindow& window, const Gui::BoardGeometry& geo, const Assets::TextureLibrary& assets,
                             const Gui::PendingPromotion& promo)
    {
        if (!promo.active)
            return;

        // Dim the whole board so the picker reads as modal.
        sf::RectangleShape dim({geo.boardWidth(), geo.boardHeight()});
        dim.setPosition(geo.boardOrigin);
        dim.setFillColor(sf::Color(0, 0, 0, 100));
        window.draw(dim);

        // Highlight the destination square so the user sees where the pawn
        // landed.
        sf::RectangleShape hl({geo.squareSize, geo.squareSize});
        hl.setPosition(geo.topLeft(promo.to));
        hl.setFillColor(sf::Color(255, 255, 0, 90));
        window.draw(hl);

        // Same order as kPromotionOrder in GuiGameController.cpp.
        constexpr PieceType order[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
        const float padding = Gui::piecePadding(geo.squareSize);

        for (int i = 0; i < 4; ++i)
        {
            const Square sq = promo.choiceSquares[i];
            if (sq == SQ_NONE)
                continue;

            sf::RectangleShape cell({geo.squareSize, geo.squareSize});
            cell.setPosition(geo.topLeft(sq));
            cell.setFillColor(sf::Color(245, 245, 245, 230));
            cell.setOutlineThickness(2.0f);
            cell.setOutlineColor(sf::Color(40, 40, 40));
            window.draw(cell);

            const Piece pc = make_piece(promo.side, order[i]);
            sf::Sprite sprite = makePieceSprite(assets, pc, geo);
            const sf::Vector2f tl = geo.topLeft(sq);
            sprite.setPosition({tl.x + padding, tl.y + padding});
            window.draw(sprite);
        }
    }

    void drawAnnotations(sf::RenderWindow& window, const Gui::BoardGeometry& geo, const Gui::BoardAnnotations& annotations,
                         const Gui::DragState& drag)
    {
        const float padding = Gui::piecePadding(geo.squareSize);

        sf::Color lastMoveColor(155, 199, 0, 120);
        for (std::optional<Square> sqOpt : {annotations.lastMoveFrom, annotations.lastMoveTo})
        {
            if (sqOpt.has_value())
            {
                sf::RectangleShape hl({geo.squareSize, geo.squareSize});
                hl.setPosition(geo.topLeft(*sqOpt));
                hl.setFillColor(lastMoveColor);
                window.draw(hl);
            }
        }

        if (annotations.selected.has_value())
        {
            sf::RectangleShape sel({geo.squareSize, geo.squareSize});
            sel.setPosition(geo.topLeft(*annotations.selected));
            sel.setFillColor(sf::Color(20, 85, 30, 100));
            sel.setOutlineThickness(3.0f);
            sel.setOutlineColor(sf::Color(20, 85, 30, 200));
            window.draw(sel);
        }

        if (annotations.selected.has_value() || drag.active)
        {
            sf::Color targetColor(0, 0, 0, 60);
            for (Square sq : annotations.legalTargets)
            {
                sf::CircleShape target(geo.squareSize * 0.15f);
                target.setFillColor(targetColor);
                sf::Vector2f tl = geo.topLeft(sq);
                // Center the circle in the square
                target.setPosition(
                    {tl.x + (geo.squareSize / 2.0f) - target.getRadius(), tl.y + (geo.squareSize / 2.0f) - target.getRadius()});
                window.draw(target);
            }
        }

        if (annotations.checkedKing.has_value())
        {
            sf::RectangleShape checkHl({geo.squareSize, geo.squareSize});
            checkHl.setPosition(geo.topLeft(*annotations.checkedKing));
            // Red gradient-like effect using a semi-transparent red overlay
            checkHl.setFillColor(sf::Color(255, 0, 0, 130));
            window.draw(checkHl);
        }
    }
} // namespace

namespace Gui
{

    void renderBoard(sf::RenderWindow& window, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const DragState& drag, const PendingPromotion& promo,
                     const BoardAnnotations& annotations)
    {
        const float padding = piecePadding(geo.squareSize);

        const sf::Texture& lightTex = assets.getTexture(theme.lightKey);
        const sf::Texture& darkTex = assets.getTexture(theme.darkKey);

        for (int i = 0; i < 64; ++i)
        {
            const Square sq = static_cast<Square>(i);
            const int file = i % 8;
            const int rank = i / 8;
            const sf::Texture& texture = ((file + rank) % 2 == 0) ? darkTex : lightTex;

            sf::Sprite square(texture);
            square.setPosition(geo.topLeft(sq));

            const auto bounds = square.getLocalBounds();
            square.setScale({geo.squareSize / bounds.size.x, geo.squareSize / bounds.size.y});

            window.draw(square);
        }
        drawAnnotations(window, geo, annotations, drag);

        // Static pieces. The dragged piece is skipped here and drawn on top below.
        for (int i = 0; i < 64; ++i)
        {
            const Square sq = static_cast<Square>(i);
            if (drag.active && sq == drag.from)
                continue;

            const Piece pc = board.pieceAt(sq);
            if (pc == PIECE_NONE)
                continue;

            sf::Sprite sprite = makePieceSprite(assets, pc, geo);
            const sf::Vector2f tl = geo.topLeft(sq);
            sprite.setPosition({tl.x + padding, tl.y + padding});
            window.draw(sprite);
        }

        if (drag.active)
        {
            const Piece pc = board.pieceAt(drag.from);
            if (pc != PIECE_NONE)
            {
                sf::Sprite sprite = makePieceSprite(assets, pc, geo);

                const sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                sprite.setPosition({static_cast<float>(mousePos.x) - drag.offset.x + padding,
                                    static_cast<float>(mousePos.y) - drag.offset.y + padding});
                window.draw(sprite);
            }
        }

        drawPromotionPicker(window, geo, assets, promo);
    }

} // namespace Gui