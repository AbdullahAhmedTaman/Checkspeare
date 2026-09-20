#include "TextureLibrary.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace Assets
{

    namespace
    {
        const char* colorKey(Color c)
        {
            return (c == WHITE) ? Key::White : Key::Black;
        }

        const char* pieceTypeKey(PieceType t)
        {
            switch (t)
            {
                case PAWN:
                    return Key::Pawn;
                case KNIGHT:
                    return Key::Knight;
                case BISHOP:
                    return Key::Bishop;
                case ROOK:
                    return Key::Rook;
                case QUEEN:
                    return Key::Queen;
                case KING:
                    return Key::King;
                default:
                    return Key::Pawn;
            }
        }
    } // namespace

    void TextureLibrary::loadAllPieceTextures(const std::string& basePath)
    {
        const char* colors[] = {Key::White, Key::Black};
        const char* types[] = {Key::Pawn, Key::Knight, Key::Bishop, Key::Rook, Key::Queen, Key::King};

        for (const char* color : colors)
        {
            for (const char* type : types)
            {
                const std::string key = std::string(color) + "_" + type;
                const std::string fullPath = basePath + key + ".png";

                sf::Texture texture;
                if (!texture.loadFromFile(fullPath))
                    throw std::runtime_error("[TextureLibrary Error] Failed to load: " + fullPath);

                m_textures[key] = std::move(texture);
            }
        }

        std::cout << "[TextureLibrary] All 12 piece textures loaded successfully!\n";
    }

    void TextureLibrary::loadAllSquareTextures(const std::string& basePath)
    {
        const char* keys[] = {Key::SquareWhite, Key::SquareBlack};

        for (const char* key : keys)
        {
            const std::string fullPath = basePath + key + ".png";

            sf::Texture texture;
            if (!texture.loadFromFile(fullPath))
                throw std::runtime_error("[TextureLibrary Error] Failed to load: " + fullPath);

            m_textures[key] = std::move(texture);
        }

        std::cout << "[TextureLibrary] All square textures loaded successfully!\n";
    }

    const sf::Texture& TextureLibrary::getTexture(const std::string& key) const
    {
        return m_textures.at(key);
    }

    std::string TextureLibrary::getPieceKey(Piece pc)
    {
        assert(pc != PIECE_NONE && "getPieceKey called with PIECE_NONE");
        return std::string(colorKey(color_of(pc))) + "_" + pieceTypeKey(type_of(pc));
    }

} // namespace Assets