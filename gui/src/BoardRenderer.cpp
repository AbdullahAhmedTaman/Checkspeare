#include "BoardRenderer.hpp"
#include "TextureLibrary.hpp"

#include <SFML/Graphics/Sprite.hpp>
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
} // namespace

namespace Gui
{

    void renderBoard(sf::RenderWindow& window, const BoardGeometry& geo, const BoardTheme& theme, const Board& board,
                     const Assets::TextureLibrary& assets, const DragState& drag)
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
    }

} // namespace Gui