#include "../include/AssetManager.h"
#include "../include/board.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

namespace Theme
{

    bool AssetManager::loadAllPieceTextures(const std::string& basePath)
    {
        const std::string colors[] = {"White", "Black"};
        const std::string types[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};

        for (const auto& color : colors)
        {
            for (const auto& type : types)
            {
                std::string key = color + "_" + type;
                std::string fullPath = basePath + key + ".png";

                sf::Texture texture;
                if (!texture.loadFromFile(fullPath))
                {
                    std::cerr << "[AssetManager Error] Failed to load: " << fullPath << '\n';
                    return false;
                }
                m_textures[key] = texture;
            }
        }
        std::cout << "[AssetManager Success] All 12 piece textures loaded successfully!\n";
        return true;
    }

    const sf::Texture& AssetManager::getTexture(const std::string& key) const
    {
        return m_textures.at(key);
    }

    std::string AssetManager::getPieceKey(const Chess::Piece& piece)
    {
        std::string colorStr = (piece.color == Chess::Color::White) ? "White" : "Black";
        std::string typeStr;

        switch (piece.type)
        {
            case Chess::PieceType::Pawn:
                typeStr = "Pawn";
                break;
            case Chess::PieceType::Knight:
                typeStr = "Knight";
                break;
            case Chess::PieceType::Bishop:
                typeStr = "Bishop";
                break;
            case Chess::PieceType::Rook:
                typeStr = "Rook";
                break;
            case Chess::PieceType::Queen:
                typeStr = "Queen";
                break;
            case Chess::PieceType::King:
                typeStr = "King";
                break;
        }
        return colorStr + "_" + typeStr;
    }
} // namespace Theme