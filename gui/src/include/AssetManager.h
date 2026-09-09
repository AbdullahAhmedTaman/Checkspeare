#pragma once
#include "board.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>

namespace Theme
{
    class AssetManager
    {
    public:
        AssetManager() = default;

        bool loadAllPieceTextures(const std::string& basePath = "gui/assets/pieces/");
        bool loadAllSquareTextures(const std::string& basePath = "gui/assets/pieces/");
        const sf::Texture& getTexture(const std::string& key) const;
        static std::string getPieceKey(const Chess::Piece& piece);

    private:
        std::unordered_map<std::string, sf::Texture> m_textures;
    };
} // namespace Theme