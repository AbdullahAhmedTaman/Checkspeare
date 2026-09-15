#pragma once
#include "SFML/Graphics/Texture.hpp"
#include "board.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Assets
{
    struct Tag
    {
        std::string Black = "Black";
        std::string White = "White";
        std::string Pawn = "Pawn";
        std::string Knight = "Knight";
        std::string Bishop = "Bishop";
        std::string Rook = "Rook";
        std::string Queen = "Queen";
        std::string King = "King";
    };

    class AssetManager
    {
    public:
        AssetManager() = default;

        void loadAllPieceTextures(const std::string& basePath = "gui/assets/pieces/");
        void loadAllSquareTextures(const std::string& basePath = "gui/assets/squares/");
        // get collection of relevant textures from a map of tag : Vector
        // Use it with The Enum for Idiot Proof
        const std::vector<const sf::Texture*>& getTextureByTag(const std::string& tag) const;
        const sf::Texture& getTexture(const std::string& key) const;
        static std::string getPieceKey(const Chess::Piece& piece);

    private:
        std::unordered_map<std::string, sf::Texture> m_textures;
        // get collection of relevant textures from a map of tag : Vector
        // memory efficient as it stores a refrence only , Texture object is VRAM heavy
        std::unordered_map<std::string, std::vector<const sf::Texture*>> m_groupTexture;
    };
} // namespace Assets