#include "TextureLibrary.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace Assets
{

    namespace
    {
        /// Filename color code: 'w' or 'b'.
        char colorCode(Color c)
        {
            return (c == WHITE) ? 'w' : 'b';
        }

        /// Filename piece code. 'N' for knight, per standard chess notation,
        /// to avoid colliding with 'K' for king.
        char pieceCode(PieceType t)
        {
            switch (t)
            {
                case PAWN:
                    return 'p';
                case KNIGHT:
                    return 'n';
                case BISHOP:
                    return 'b';
                case ROOK:
                    return 'r';
                case QUEEN:
                    return 'q';
                case KING:
                    return 'k';
                default:
                    return 'p';
            }
        }
    } // namespace

    void TextureLibrary::loadAllPieceTextures(const std::filesystem::path& basePath)
    {
        constexpr Color colors[] = {WHITE, BLACK};
        constexpr PieceType types[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

        for (Color color : colors)
        {
            for (PieceType type : types)
            {
                const std::string key = std::string(1, colorCode(color)) + pieceCode(type);
                const std::filesystem::path fullPath = basePath / (key + ".png");

                sf::Texture texture;
                if (!texture.loadFromFile(fullPath))
                    throw std::runtime_error("[TextureLibrary Error] Failed to load: " + fullPath.string());
                texture.setSmooth(true);
                m_textures[key] = std::move(texture);
            }
        }

        std::cout << "[TextureLibrary] All 12 piece textures loaded successfully!\n";
    }

    void TextureLibrary::loadAllSquareTextures(const std::filesystem::path& basePath)
    {
        const char* keys[] = {Key::SquareWhite, Key::SquareBlack};

        for (const char* key : keys)
        {
            const std::filesystem::path fullPath = basePath / (std::string(key) + ".png");

            sf::Texture texture;
            if (!texture.loadFromFile(fullPath))
                throw std::runtime_error("[TextureLibrary Error] Failed to load: " + fullPath.string());
            texture.setSmooth(true);
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
        return std::string(1, colorCode(color_of(pc))) + pieceCode(type_of(pc));
    }

} // namespace Assets