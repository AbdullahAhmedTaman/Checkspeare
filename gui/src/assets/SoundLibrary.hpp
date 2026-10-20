#pragma once

#include <SFML/Audio/SoundBuffer.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>


namespace Assets
{

    namespace SoundKey
    {
        inline constexpr const char* Move = "move";
        inline constexpr const char* Capture = "capture";
        inline constexpr const char* Check = "check";
        inline constexpr const char* Castle = "castle";
        inline constexpr const char* Promote = "promote";

        inline constexpr const char* Victory = "victory";
        inline constexpr const char* Defeat = "defeat";
        inline constexpr const char* Draw = "draw";

        inline constexpr const char* Illegal = "illegal";
        inline constexpr const char* Cancel = "cancel";
    } // namespace SoundKey

    /// Owns sound effects and exposes them by category. Each category holds
    /// one or more variants; playing a category picks a random variant, so
    /// repeated plays of the same kind don't sound identical.
    ///
    /// Files are expected as `<basePath><name>_1.wav`, `_2.wav`, etc.
    /// Loading stops at the first missing index, so a category can have any
    /// number of variants (at least one).
    class SoundLibrary
    {
    public:
        SoundLibrary() = default;

        SoundLibrary(const SoundLibrary&) = delete;
        SoundLibrary& operator=(const SoundLibrary&) = delete;
        SoundLibrary(SoundLibrary&&) = delete;
        SoundLibrary& operator=(SoundLibrary&&) = delete;

        /// Loads every category the game uses. Throws if any category is
        /// missing at least one variant.
        void loadAll(const std::filesystem::path& basePath);

        /// Loads a single category. Throws if no variants are found.
        void loadCategory(const std::filesystem::path& basePath, const std::string& name);

        /// Random variant of the named category. Throws if the category
        /// was never loaded.
        const sf::SoundBuffer& randomBuffer(const std::string& name) const;

    private:
        std::unordered_map<std::string, std::vector<sf::SoundBuffer>> m_buffers;
    };

} // namespace Assets