#include "../include/AssetManager.h"
#include "../include/board.h"
#include "SFML/Graphics/Texture.hpp"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

namespace AssetManager
{

    void AssetManager::loadAllPieceTextures(const std::string& basePath)
    {
        // loockup tables
        const std::string colors[] = {"White", "Black"};
        const std::string types[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};

        for (const auto& color : colors)
        {
            for (const auto& type : types)
            {
                // the base path which ends wit '/' so the loadFromFile is accessable
                std::string key = color + "_" + type;
                std::string fullPath = basePath + key + ".png";

                sf::Texture texture;
                if (!texture.loadFromFile(fullPath))
                {
                    throw std::runtime_error("[AssetManager Error] Failed to load:" + fullPath + '\n');
                }

                m_textures[key] = texture;
                // class by color
                m_groupTexture[color].push_back(&m_textures[key]);
                // class by type
                m_groupTexture[type].push_back(&m_textures[key]);
            }
        }
        std::cout << "[AssetManager Success] All 12 piece textures loaded successfully!\n";
    }

    void AssetManager::loadAllSquareTextures(const std::string& basePath)
    {
        const std::string colors[] = {"White", "Black"};
        for (const auto& color : colors)
        {
            std::string key = "Square_" + color;
            std::string fullPath = basePath + key + ".png";

            sf ::Texture texture;
            if (!texture.loadFromFile(fullPath))
            {
                throw std::runtime_error("[AssetManager Error] Failed to load:" + fullPath + '\n');
            }
            m_textures[key] = texture;
        }
        std::cout << "[AssetManager Success] All Square textures loaded successfully!\n";
    }

    const sf::Texture& AssetManager::getTexture(const std::string& key) const
    {
        return m_textures.at(key);
    }

    const std::vector<const sf::Texture*>& AssetManager::getTextureByTag(const std::string& tag) const
    {
        // use the Tag Enum for Ediot Proof
        return m_groupTexture.at(tag);
    }

    // regenerate the Key from Pieces by passing an actual Piece object
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
} // namespace AssetManager