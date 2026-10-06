#include "BoardRenderer.hpp"
#include "../../assets/TextureLibrary.hpp"
#include "BoardAnnotations.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    // Subtle drop shadow for resting pieces.
    constexpr float kShadowOffset = 0.045f;
    constexpr std::uint8_t kShadowAlpha = 60;

    // A lifted piece (drag) gets a slightly stronger shadow.
    constexpr float kDragShadowOffset = 0.075f;
    constexpr std::uint8_t kDragShadowAlpha = 90;

    sf::Sprite makePieceSprite(const Assets::TextureLibrary& assets, Piece pc, const Gui::BoardGeometry& geo)
    {
        const sf::Texture& tex = assets.getTexture(Assets::TextureLibrary::getPieceKey(pc));
        sf::Sprite sprite(tex);

        const float targetSize = geo.squareSize * Gui::PieceSpriteScale;
        const auto bounds = sprite.getLocalBounds();
        sprite.setScale({targetSize / bounds.size.x, targetSize / bounds.size.y});
        return sprite;
    }

    // Draws a piece with a soft drop shadow. The shadow is built from a
    // few low-alpha silhouette passes that fade and spread along the
    // light direction, approximating a blur without a shader.
    void drawPieceWithShadow(sf::RenderTarget& target, const Assets::TextureLibrary& assets, Piece pc,
                             const Gui::BoardGeometry& geo, sf::Vector2f pos, float offsetFactor = kShadowOffset,
                             std::uint8_t shadowAlpha = kShadowAlpha)
    {
        sf::Sprite sprite = makePieceSprite(assets, pc, geo);
        const sf::Vector2f baseScale = sprite.getScale();
        const float offset = geo.squareSize * offsetFactor;

        constexpr int kPasses = 5;
        const std::uint8_t passAlpha = static_cast<std::uint8_t>(shadowAlpha / (kPasses + 1));

        // Passes run from the outermost (faintest, most spread) inward.
        for (int i = kPasses; i >= 1; --i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(kPasses);
            const float d = offset * t;
            const float spread = 1.0f + 0.05f * t;

            sprite.setScale({baseScale.x * spread, baseScale.y * spread});
            sprite.setPosition({pos.x + d, pos.y + d});
            sprite.setColor(sf::Color(0, 0, 0, passAlpha));
            target.draw(sprite);
        }

        sprite.setScale(baseScale);
        sprite.setPosition(pos);
        sprite.setColor(sf::Color::White);
        target.draw(sprite);
    }

    // Draws a square outline that sits *entirely inside* a board square.
    void drawSquareOutline(sf::RenderTarget& target, const Gui::BoardGeometry& geo, Square sq, float thickness, sf::Color color)
    {
        sf::RectangleShape outline({geo.squareSize - 2.0f * thickness, geo.squareSize - 2.0f * thickness});
        outline.setPosition(geo.topLeft(sq) + sf::Vector2f(thickness, thickness));
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineThickness(thickness);
        outline.setOutlineColor(color);
        target.draw(outline);
    }

    // Draws four L-shaped corner brackets just inside a square's border.
    // Used as a capture indicator: it reads instantly as "capture target"
    // without covering the piece underneath the way a filled circle would.
    void drawSquareCorners(sf::RenderTarget& target, const Gui::BoardGeometry& geo, Square sq, float thickness, float length,
                           sf::Color color)
    {
        const sf::Vector2f tl = geo.topLeft(sq);
        const float s = geo.squareSize;
        const float pad = s * 0.08f; // sits fully inside the square

        auto bar = [&](float x, float y, float w, float h)
        {
            sf::RectangleShape r({w, h});
            r.setPosition({tl.x + x, tl.y + y});
            r.setFillColor(color);
            target.draw(r);
        };

        // Top-left
        bar(pad, pad, length, thickness);
        bar(pad, pad, thickness, length);
        // Top-right
        bar(s - pad - length, pad, length, thickness);
        bar(s - pad - thickness, pad, thickness, length);
        // Bottom-left
        bar(pad, s - pad - thickness, length, thickness);
        bar(pad, s - pad - length, thickness, length);
        // Bottom-right
        bar(s - pad - length, s - pad - thickness, length, thickness);
        bar(s - pad - thickness, s - pad - length, thickness, length);
    }

    void drawPromotionPicker(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const Assets::TextureLibrary& assets,
                             const Gui::PendingPromotion& promo)
    {
        if (!promo.active)
            return;

        // Dim the board to make the picker modal.
        sf::RectangleShape dim({geo.boardWidth(), geo.boardHeight()});
        dim.setPosition(geo.boardOrigin);
        dim.setFillColor(sf::Color(0, 0, 0, 100));
        target.draw(dim);

        // Highlight the destination square.
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
            target.draw(cell);

            drawSquareOutline(target, geo, sq, 2.0f, sf::Color(40, 40, 40));

            const Piece pc = make_piece(promo.side, order[i]);
            const sf::Vector2f tl = geo.topLeft(sq);
            drawPieceWithShadow(target, assets, pc, geo, {tl.x + padding, tl.y + padding});
        }
    }

    void drawAnnotations(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const Gui::BoardAnnotations& annotations,
                         const Gui::DragState& drag, const Board& board)
    {
        // Last move: soft yellow on origin, stronger on destination, plus border.
        const sf::Color fromColor(180, 200, 60, 70);
        const sf::Color toColor(180, 200, 60, 120);
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

            drawSquareOutline(target, geo, *annotations.lastMoveTo, 3.0f, toBorder);
        }

        if (annotations.selected.has_value())
        {
            sf::RectangleShape selFill({geo.squareSize, geo.squareSize});
            selFill.setPosition(geo.topLeft(*annotations.selected));
            selFill.setFillColor(sf::Color(20, 85, 30, 100));
            target.draw(selFill);

            drawSquareOutline(target, geo, *annotations.selected, 4.0f, sf::Color(20, 85, 30, 200));
        }

        if (annotations.selected.has_value() || drag.active)
        {
            for (Square sq : annotations.legalTargets)
            {
                const sf::Vector2f tl = geo.topLeft(sq);

                // Capture if an enemy piece occupies the square. En passant
                // renders as a quiet move (minor visual mis-classification).
                const bool isCapture = board.pieceAt(sq) != PIECE_NONE;

                if (isCapture)
                {
                    drawSquareCorners(target, geo, sq, geo.squareSize * 0.06f, geo.squareSize * 0.22f,
                                      sf::Color(200, 60, 60, 200));
                }
                else
                {
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
            checkHl.setFillColor(sf::Color(255, 0, 0, 130));
            target.draw(checkHl);
        }
    }

    // While a piece is being dragged, outline the square under the pointer
    // so the player can see where it would be dropped.
    void drawDragTargetHighlight(sf::RenderTarget& target, const Gui::BoardGeometry& geo, sf::Vector2f pixelPos)
    {
        const sf::Vector2f local = pixelPos - geo.boardOrigin;
        const int gridX = static_cast<int>(std::floor(local.x / geo.squareSize));
        const int gridY = static_cast<int>(std::floor(local.y / geo.squareSize));

        if (gridX < 0 || gridX >= 8 || gridY < 0 || gridY >= 8)
            return;

        const Square sq = Gui::BoardGeometry::gridToSquare(gridX, gridY, geo.flipped);
        if (sq == SQ_NONE)
            return;

        const float thickness = std::max(1.0f, geo.squareSize * 0.06f);
        drawSquareOutline(target, geo, sq, thickness, sf::Color(255, 255, 255, 160));
    }

    // Draws a–h and 1–8 labels along bottom and left edges.
    // Contrast chosen per-square: light labels on dark squares, dark on light.
    void drawCoordinateLabels(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const sf::Font& font)
    {
        const float fontSize = geo.squareSize * 0.18f;
        const float pad = geo.squareSize * 0.04f;

        sf::Text text(font);
        text.setCharacterSize(static_cast<unsigned>(fontSize));
        text.setStyle(sf::Text::Style::Bold);

        const sf::Color lightText(245, 245, 245, 230);
        const sf::Color darkText(30, 30, 30, 230);

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

        // Files: a..h along the bottom edge (grid row 7).
        for (int f = 0; f < 8; ++f)
        {
            const int gridX = geo.flipped ? 7 - f : f;
            const int gridY = 7;

            const sf::Vector2f tl(geo.boardOrigin.x + gridX * geo.squareSize, geo.boardOrigin.y + gridY * geo.squareSize);

            text.setString(std::string(1, static_cast<char>('a' + f)));
            text.setFillColor(colorAtGrid(gridX, gridY));

            const sf::FloatRect b = text.getLocalBounds();
            text.setPosition(
                {tl.x + geo.squareSize - pad - b.size.x - b.position.x, tl.y + geo.squareSize - pad - b.size.y - b.position.y});
            target.draw(text);
        }

        // Ranks: 1..8 along the left edge (grid column 0).
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

    bool isKnightOffset(Square from, Square to)
    {
        const int fx = static_cast<int>(from) % 8;
        const int fy = static_cast<int>(from) / 8;
        const int tx = static_cast<int>(to) % 8;
        const int ty = static_cast<int>(to) / 8;

        const int dx = std::abs(tx - fx);
        const int dy = std::abs(ty - fy);

        return (dx == 2 && dy == 1) || (dx == 1 && dy == 2);
    }

    /// Draws one straight shaft + head from `fromPt` to `toPt` in `fill`.
    void drawStraightArrow(sf::RenderTarget& target, sf::Vector2f fromPt, sf::Vector2f toPt, sf::Color fill, float squareSize)
    {
        const float shaftWidth = squareSize * 0.13f;
        const float headLength = squareSize * 0.34f;
        const float headWidth = squareSize * 0.34f;

        const sf::Vector2f delta = toPt - fromPt;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length < 1.0f)
            return;

        const sf::Vector2f dir = delta / length;
        const sf::Vector2f perp(-dir.y, dir.x);

        const sf::Vector2f shaftEnd = toPt - dir * headLength;
        const sf::Vector2f half = perp * (shaftWidth * 0.5f);

        sf::ConvexShape shaft(4);
        shaft.setPoint(0, fromPt + half);
        shaft.setPoint(1, shaftEnd + half);
        shaft.setPoint(2, shaftEnd - half);
        shaft.setPoint(3, fromPt - half);
        shaft.setFillColor(fill);
        target.draw(shaft);

        const sf::Vector2f headBase = toPt - dir * headLength;
        const sf::Vector2f headHalf = perp * (headWidth * 0.5f);

        sf::ConvexShape head(3);
        head.setPoint(0, toPt);
        head.setPoint(1, headBase + headHalf);
        head.setPoint(2, headBase - headHalf);
        head.setFillColor(fill);
        target.draw(head);
    }

    void drawArrow(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const Gui::Arrow& a)
    {
        if (a.from == SQ_NONE || a.to == SQ_NONE || a.from == a.to)
            return;

        const sf::Color fill(232, 168, 56, 210);

        const sf::Vector2f fromCenter = geo.topLeft(a.from) + sf::Vector2f(geo.squareSize * 0.5f, geo.squareSize * 0.5f);
        const sf::Vector2f toCenter = geo.topLeft(a.to) + sf::Vector2f(geo.squareSize * 0.5f, geo.squareSize * 0.5f);

        // Knight moves are drawn as an L: first the long leg (2 squares along
        // one axis), then the short leg (1 square along the other). The
        // arrowhead sits at the end of the short leg. Any other move is drawn
        // straight.
        if (isKnightOffset(a.from, a.to))
        {
            const int fx = static_cast<int>(a.from) % 8;
            const int fy = static_cast<int>(a.from) / 8;
            const int tx = static_cast<int>(a.to) % 8;
            const int ty = static_cast<int>(a.to) / 8;

            const int dx = tx - fx;
            const int dy = ty - fy;

            // The long leg runs along whichever axis has magnitude 2. The
            // corner sits at the intersection of that axis with the other.
            sf::Vector2f corner;
            if (std::abs(dx) == 2)
            {
                // Horizontal long leg first, then vertical short leg.
                corner = geo.topLeft(a.from) + sf::Vector2f(geo.squareSize * 0.5f, geo.squareSize * 0.5f);
                corner.x += static_cast<float>(dx) * geo.squareSize;
            }
            else
            {
                // Vertical long leg first, then horizontal short leg.
                corner = geo.topLeft(a.from) + sf::Vector2f(geo.squareSize * 0.5f, geo.squareSize * 0.5f);
                corner.y += static_cast<float>(dy) * geo.squareSize;
            }

            // Two straight segments. The first has no head (it stops at the
            // corner), the second carries the arrowhead into the destination.
            drawStraightArrow(target, fromCenter, corner, fill, geo.squareSize);
            drawStraightArrow(target, corner, toCenter, fill, geo.squareSize);
        }
        else
        {
            drawStraightArrow(target, fromCenter, toCenter, fill, geo.squareSize);
        }
    }

    void drawArrows(sf::RenderTarget& target, const Gui::BoardGeometry& geo, const std::vector<Gui::Arrow>& arrows)
    {
        for (const Gui::Arrow& a : arrows)
            drawArrow(target, geo, a);
    }
} // namespace

namespace Gui
{
    void renderBoard(sf::RenderTarget& target, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const sf::Font& coordinateFont, const DragState& drag,
                     sf::Vector2f dragPixelPos, const PendingPromotion& promo, const BoardAnnotations& annotations,
                     const MoveAnimation& animation, const std::vector<Arrow>& arrows)
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

        if (drag.active)
            drawDragTargetHighlight(target, geo, dragPixelPos);

        drawCoordinateLabels(target, geo, coordinateFont);

        // Static pieces. Skip dragged piece and in-flight destination.
        for (int i = 0; i < 64; ++i)
        {
            const Square sq = static_cast<Square>(i);
            if (drag.active && sq == drag.from)
                continue;

            if (animation.active && sq == animation.to)
                continue;

            const Piece pc = board.pieceAt(sq);
            if (pc == PIECE_NONE)
                continue;

            const sf::Vector2f tl = geo.topLeft(sq);
            drawPieceWithShadow(target, assets, pc, geo, {tl.x + padding, tl.y + padding});
        }

        // In-flight piece (above static pieces, below active drag).
        if (animation.active && animation.piece != PIECE_NONE)
        {
            const sf::Vector2f fromPos = geo.topLeft(animation.from);
            const sf::Vector2f toPos = geo.topLeft(animation.to);

            const float t = animation.easedT();
            const float x = fromPos.x + (toPos.x - fromPos.x) * t;
            const float y = fromPos.y + (toPos.y - fromPos.y) * t;

            drawPieceWithShadow(target, assets, animation.piece, geo, {x + padding, y + padding});
        }

        if (drag.active)
        {
            const Piece pc = board.pieceAt(drag.from);
            if (pc != PIECE_NONE)
            {
                drawPieceWithShadow(target, assets, pc, geo,
                                    {dragPixelPos.x - drag.offset.x + padding, dragPixelPos.y - drag.offset.y + padding},
                                    kDragShadowOffset, kDragShadowAlpha);
            }
        }

        drawPromotionPicker(target, geo, assets, promo);
        drawArrows(target, geo, arrows);
    }
} // namespace Gui