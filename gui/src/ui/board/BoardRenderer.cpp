#include "BoardRenderer.hpp"
#include "BoardAnnotations.hpp"
#include "../../assets/TextureLibrary.hpp"

#include <SFML/Graphics.hpp>

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

    void drawPromotionPicker(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const Assets::TextureLibrary& assets,
                             const Gui::PendingPromotion& promo)
    {
        if (!promo.active)
            return;

        // Dim the whole board so the picker reads as modal.
        sf::RectangleShape dim({geo.boardWidth(), geo.boardHeight()});
        dim.setPosition(geo.boardOrigin);
        dim.setFillColor(sf::Color(0, 0, 0, 100));
        target.draw(dim);

        // Highlight the destination square so the user sees where the pawn
        // landed.
        sf::RectangleShape hl({geo.squareSize, geo.squareSize});
        hl.setPosition(geo.topLeft(promo.to));
        hl.setFillColor(sf::Color(255, 255, 0, 90));
        target.draw(hl);

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
            target.draw(cell);

            const Piece pc = make_piece(promo.side, order[i]);
            sf::Sprite sprite = makePieceSprite(assets, pc, geo);
            const sf::Vector2f tl = geo.topLeft(sq);
            sprite.setPosition({tl.x + padding, tl.y + padding});
            target.draw(sprite);
        }
    }

    void drawAnnotations(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const Gui::BoardAnnotations& annotations,
                         const Gui::DragState& drag, const Board& board)
    {
        // Last move: soft yellow on the origin square, stronger on the
        // destination, plus a thin border on the destination so the eye is
        // drawn to where the piece landed rather than where it came from.
        const sf::Color fromColor(180, 200, 60, 70);
        const sf::Color toColor(180, 200, 60, 110);
        const sf::Color toBorder(150, 170, 40, 200);

        if (annotations.lastMoveFrom.has_value())
        {
            sf::RectangleShape hl({geo.squareSize, geo.squareSize});
            hl.setPosition(geo.topLeft(*annotations.lastMoveFrom));
            hl.setFillColor(fromColor);
            target.draw(hl);
        }

        if (annotations.lastMoveTo.has_value())
        {
            const sf::Vector2f tl = geo.topLeft(*annotations.lastMoveTo);

            sf::RectangleShape hl({geo.squareSize, geo.squareSize});
            hl.setPosition(tl);
            hl.setFillColor(toColor);
            target.draw(hl);

            sf::RectangleShape border({geo.squareSize - 4.0f, geo.squareSize - 4.0f});
            border.setPosition({tl.x + 2.0f, tl.y + 2.0f});
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineThickness(2.0f);
            border.setOutlineColor(toBorder);
            target.draw(border);
        }

        if (annotations.selected.has_value())
        {
            sf::RectangleShape sel({geo.squareSize, geo.squareSize});
            sel.setPosition(geo.topLeft(*annotations.selected));
            sel.setFillColor(sf::Color(20, 85, 30, 100));
            sel.setOutlineThickness(3.0f);
            sel.setOutlineColor(sf::Color(20, 85, 30, 200));
            target.draw(sel);
        }

        if (annotations.selected.has_value() || drag.active)
        {
            for (Square sq : annotations.legalTargets)
            {
                const sf::Vector2f tl = geo.topLeft(sq);

                // A destination holding an enemy piece is a capture. The
                // one exception is en passant, since the captured pawn is
                // not on `to`; it will render as a quiet move, which is a
                // minor visual mis-classification of a rare case.
                const bool isCapture = board.pieceAt(sq) != PIECE_NONE;

                if (isCapture)
                {
                    // Capture: hollow square frame nearly filling the
                    // square. Reads as a target and stands out more than
                    // the quiet-move dot because it has more surface area.
                    const float inset = geo.squareSize * 0.1f;

                    sf::RectangleShape frame({geo.squareSize - inset * 2.0f, geo.squareSize - inset * 2.0f});
                    frame.setPosition({tl.x + inset, tl.y + inset});
                    frame.setFillColor(sf::Color::Transparent);
                    frame.setOutlineThickness(geo.squareSize * 0.1f);
                    frame.setOutlineColor(sf::Color(160, 60, 60, 120));
                    sf::CircleShape marker(geo.squareSize * 0.22f);
                    marker.setFillColor(sf::Color(180, 60, 60, 120));
                    marker.setPosition({tl.x + (geo.squareSize / 2.0f) - marker.getRadius(),
                                        tl.y + (geo.squareSize / 2.0f) - marker.getRadius()});
                    target.draw(marker);
                    target.draw(frame);
                }
                else
                {
                    // Quiet move: small centered dot, unchanged.
                    sf::CircleShape marker(geo.squareSize * 0.17f);
                    marker.setFillColor(sf::Color(0, 0, 0, 90));
                    marker.setPosition({tl.x + (geo.squareSize / 2.0f) - marker.getRadius(),
                                        tl.y + (geo.squareSize / 2.0f) - marker.getRadius()});
                    target.draw(marker);
                }
            }
        }

        if (annotations.checkedKing.has_value())
        {
            sf::RectangleShape checkHl({geo.squareSize, geo.squareSize});
            checkHl.setPosition(geo.topLeft(*annotations.checkedKing));
            // Red gradient-like effect using a semi-transparent red overlay
            checkHl.setFillColor(sf::Color(255, 0, 0, 130));
            target.draw(checkHl);
        }
    }

    // Draws the a–h and 1–8 labels along the bottom and left edges of the
    /// visible board. Called after pieces so the labels stay legible even
    /// when a piece sprite overhangs its square.
    ///
    /// Contrast is chosen per-square: labels on dark squares are drawn
    /// light, labels on light squares are drawn dark. This matches how
    /// chess.com and lichess render coordinates and keeps the text
    /// readable regardless of the underlying square's color.
    void drawCoordinateLabels(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const sf::Font& font)
    {
        // 22% of a square is a good visual weight: big enough to read at
        // a glance, small enough not to compete with the pieces.
        const float fontSize = geo.squareSize * 0.18f;
        const float pad = geo.squareSize * 0.05f;

        sf::Text text(font);
        text.setCharacterSize(static_cast<unsigned>(fontSize));
        text.setStyle(sf::Text::Style::Bold);

        // Two text colors: one for dark squares, one for light. Alpha is
        // not quite full so the labels read as a subtle watermark rather
        // than as content on the board.
        const sf::Color lightText(245, 245, 245, 230);
        const sf::Color darkText(30, 30, 30, 230);

        // Determines the label color for a given grid cell by inspecting
        // the chess coordinates of the square underneath it. "Dark" is
        // (file + rank) even, matching how BoardRenderer picks the square
        // texture: `((file + rank) % 2 == 0) ? darkTex : lightTex`.
        auto colorAtGrid = [&](int gridX, int gridY) -> sf::Color
        {
            const Square sq = Gui::BoardGeometry::gridToSquare(gridX, gridY, geo.flipped);
            if (sq == SQ_NONE)
                return darkText;

            const int file = static_cast<int>(sq) % 8;
            const int rank = static_cast<int>(sq) / 8;
            const bool isDark = ((file + rank) % 2) == 0;

            return isDark ? lightText : darkText;
        };

        // Files: a..h. Drawn along the bottom edge of the *visible* board,
        // which is grid row 7 regardless of orientation — `geo.flipped`
        // only affects which board file lands at each grid column.
        for (int f = 0; f < 8; ++f)
        {
            const int gridX = geo.flipped ? 7 - f : f;
            const int gridY = 7;

            const sf::Vector2f tl(geo.boardOrigin.x + gridX * geo.squareSize, geo.boardOrigin.y + gridY * geo.squareSize);

            text.setString(std::string(1, static_cast<char>('A' + f)));
            text.setFillColor(colorAtGrid(gridX, gridY));

            // SFML's Text local origin is not the top-left of the drawn
            // glyphs; getLocalBounds().position is that offset. The idiom
            // is to subtract it so setPosition places the bounding box,
            // not the baseline-anchored origin.
            const sf::FloatRect b = text.getLocalBounds();
            text.setPosition(
                {tl.x + geo.squareSize - pad - b.size.x - b.position.x, tl.y + geo.squareSize - pad - b.size.y - b.position.y});
            target.draw(text);
        }

        // Ranks: 1..8. Drawn along the left edge of the visible board,
        // which is grid column 0. `geo.flipped` decides which board rank
        // lands on each grid row.
        for (int r = 0; r < 8; ++r)
        {
            const int gridY = geo.flipped ? r : 7 - r;
            const int gridX = 0;

            const sf::Vector2f tl(geo.boardOrigin.x + gridX * geo.squareSize, geo.boardOrigin.y + gridY * geo.squareSize);

            text.setString(std::to_string(r + 1));
            text.setFillColor(colorAtGrid(gridX, gridY));

            const sf::FloatRect b = text.getLocalBounds();
            text.setPosition({tl.x + pad - b.position.x, tl.y + pad - b.position.y});
            target.draw(text);
        }
    }
} // namespace

namespace Gui
{

    void renderBoard(sf::RenderTarget& target, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const sf::Font& coordinateFont, const DragState& drag,
                     sf::Vector2f dragPixelPos, const PendingPromotion& promo, const BoardAnnotations& annotations,
                     const MoveAnimation& animation)
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

            target.draw(square);
        }
        drawAnnotations(target, geo, annotations, drag, board);
        // Static pieces. The dragged piece is skipped here and drawn on top below.
        for (int i = 0; i < 64; ++i)
        {
            const Square sq = static_cast<Square>(i);
            if (drag.active && sq == drag.from)
                continue;

            // The in-flight piece is drawn interpolated below; skipping it
            // here prevents overlap with the destination sprite.
            if (animation.active && sq == animation.to)
                continue;

            const Piece pc = board.pieceAt(sq);
            if (pc == PIECE_NONE)
                continue;

            sf::Sprite sprite = makePieceSprite(assets, pc, geo);
            const sf::Vector2f tl = geo.topLeft(sq);
            sprite.setPosition({tl.x + padding, tl.y + padding});
            target.draw(sprite);
        }

        // In-flight piece: drawn above static pieces, below an active drag.
        if (animation.active && animation.piece != PIECE_NONE)
        {
            const sf::Vector2f fromPos = geo.topLeft(animation.from);
            const sf::Vector2f toPos = geo.topLeft(animation.to);

            const float t = animation.easedT();
            const float x = fromPos.x + (toPos.x - fromPos.x) * t;
            const float y = fromPos.y + (toPos.y - fromPos.y) * t;

            sf::Sprite sprite = makePieceSprite(assets, animation.piece, geo);
            sprite.setPosition({x + padding, y + padding});
            target.draw(sprite);
        }

        if (drag.active)
        {
            const Piece pc = board.pieceAt(drag.from);
            if (pc != PIECE_NONE)
            {
                sf::Sprite sprite = makePieceSprite(assets, pc, geo);

                // The pointer position is now supplied by the caller, so the
                // renderer no longer reaches back into sf::Mouse. This is
                // what lets the board be drawn into a RenderTexture.
                sprite.setPosition({dragPixelPos.x - drag.offset.x + padding, dragPixelPos.y - drag.offset.y + padding});
                target.draw(sprite);
            }
        }

        drawPromotionPicker(target, geo, assets, promo);
        drawCoordinateLabels(target, geo, coordinateFont);
    }

} // namespace Gui