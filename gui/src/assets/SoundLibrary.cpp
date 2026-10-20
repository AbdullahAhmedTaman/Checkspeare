#include "SoundLibrary.hpp"

#include <filesystem>
#include <random>
#include <stdexcept>

namespace Assets
{

    namespace
    {
        std::mt19937& rng()
        {
            static std::mt19937 gen{std::random_device{}()};
            return gen;
        }
    } // namespace

    void SoundLibrary::loadAll(const std::filesystem::path& basePath)
    {
        loadCategory(basePath, SoundKey::Move);
        loadCategory(basePath, SoundKey::Capture);
        loadCategory(basePath, SoundKey::Check);
        loadCategory(basePath, SoundKey::Castle);
        loadCategory(basePath, SoundKey::Promote);
        loadCategory(basePath, SoundKey::Victory);
        loadCategory(basePath, SoundKey::Defeat);
        loadCategory(basePath, SoundKey::Draw);
        loadCategory(basePath, SoundKey::Illegal);
        loadCategory(basePath, SoundKey::Cancel);
    }

    void SoundLibrary::loadCategory(const std::filesystem::path& basePath, const std::string& name)
    {
        std::vector<sf::SoundBuffer> variants;

        for (int i = 1;; ++i)
        {
            const std::filesystem::path fullPath = basePath / (name + "_" + std::to_string(i) + ".wav");

            // SFML's loadFromFile logs to stderr on every failed open. The
            // probe loop relies on failure to know when variants stop, so
            // check existence first to keep the probe silent.
            std::error_code ec;
            if (!std::filesystem::exists(fullPath, ec))
                break;

            sf::SoundBuffer buffer;
            if (!buffer.loadFromFile(fullPath))
                break;

            variants.push_back(std::move(buffer));
        }

        if (variants.empty())
            throw std::runtime_error("[SoundLibrary] No variants found for: " + name);

        m_buffers[name] = std::move(variants);
    }

    const sf::SoundBuffer& SoundLibrary::randomBuffer(const std::string& name) const
    {
        const auto it = m_buffers.find(name);
        if (it == m_buffers.end() || it->second.empty())
            throw std::runtime_error("[SoundLibrary] Category not loaded: " + name);

        const auto& variants = it->second;
        std::uniform_int_distribution<std::size_t> dist(0, variants.size() - 1);
        return variants[dist(rng())];
    }

} // namespace Assets