#pragma once

#include "types.h"

#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <unordered_map>

namespace Assets
{

    namespace Key
    {
        inline constexpr const char* White = "White";
        inline constexpr const char* Black = "Black";

        inline constexpr const char* Pawn = "Pawn";
        inline constexpr const char* Knight = "Knight";
        inline constexpr const char* Bishop = "Bishop";
        inline constexpr const char* Rook = "Rook";
        inline constexpr const char* Queen = "Queen";
        inline constexpr const char* King = "King";

        inline constexpr const char* SquareWhite = "Square_White";
        inline constexpr const char* SquareBlack = "Square_Black";
    } // namespace Key

    /// Owns SFML textures and exposes stable references into them.
    ///
    /// Lifetime: a reference returned by getTexture is valid until the next
    /// load* call or until the manager is destroyed.
    class TextureLibrary

    {
    public:
        TextureLibrary() = default;

        TextureLibrary(const TextureLibrary&) = delete;
        TextureLibrary& operator=(const TextureLibrary&) = delete;
        TextureLibrary(TextureLibrary&&) = delete;
        TextureLibrary& operator=(TextureLibrary&&) = delete;

        /// Loads the 12 standard piece textures from `basePath`, which must
        /// end with '/'. Idempotent.
        void loadAllPieceTextures(const std::string& basePath);

        /// Loads Square_White.png and Square_Black.png from `basePath`,
        /// which must end with '/'.
        void loadAllSquareTextures(const std::string& basePath);

        /// Throws std::out_of_range if `key` was not loaded.
        const sf::Texture& getTexture(const std::string& key) const;

        /// Builds the texture key for an engine piece, e.g. "White_Knight".
        /// Precondition: pc != PIECE_NONE.
        static std::string getPieceKey(Piece pc);

    private:
        std::unordered_map<std::string, sf::Texture> m_textures;
    };

} // namespace Assets