#include "BoardRenderer.hpp"
#include "AssetsManager.h"

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Mouse.hpp>

namespace
{
    sf::Sprite makePieceSprite(const Assets::AssetManager& assets, Piece pc, const Gui::BoardGeometry& geo)
    {
        const sf::Texture& tex = assets.getTexture(Assets::AssetManager::getPieceKey(pc));
        sf::Sprite sprite(tex);

        const float targetSize = geo.squareSize * Gui::PieceSpriteScale;
        const auto bounds = sprite.getLocalBounds();
        sprite.setScale({targetSize / bounds.size.x, targetSize / bounds.size.y});
        return sprite;
    }
} // namespace

namespace Gui
{

    void renderBoard(sf::RenderWindow& window, const BoardGeometry& geo, const BoardTheme& theme, const ChessGame& game,
                     const Assets::AssetManager& assets, const DragState& drag)
    {
        const float padding = piecePadding(geo.squareSize);

        const sf::Texture& lightTex = assets.getTexture(theme.lightKey);
        const sf::Texture& darkTex = assets.getTexture(theme.darkKey);

        // ---- Squares --------------------------------------------------------
        // Iterate over real squares rather than screen cells so the light/dark
        // pattern stays tied to the board itself, and the flip is expressed
        // entirely through geo.topLeft().
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
        // ---- Static pieces --------------------------------------------------
        for (int i = 0; i < 64; ++i)
        {
            const Square sq = static_cast<Square>(i);
            if (drag.active && sq == drag.from)
                continue;

            const Piece pc = game.pieceAt(sq);
            if (pc == PIECE_NONE)
                continue;

            sf::Sprite sprite = makePieceSprite(assets, pc, geo);
            const sf::Vector2f tl = geo.topLeft(sq);
            sprite.setPosition({tl.x + padding, tl.y + padding});
            window.draw(sprite);
        }

        // ---- Dragged piece (on top) ----------------------------------------
        if (drag.active)
        {
            const Piece pc = game.pieceAt(drag.from);
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