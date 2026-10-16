#pragma once

#include "assets/SoundLibrary.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sf
{
    class Sound;
} // namespace sf

/// Plays fire-and-forget sounds from a SoundLibrary. Each call picks a
/// random variant of the named category. A small pool of sf::Sound objects
/// is reused; the pool grows only when every slot is busy, which bounds
/// object churn during rapid bursts.
///
/// Lifetime: holds a reference to the library. The library must outlive
/// the player, and every buffer it hands out must remain valid for the
/// duration of any sound that references it. Since SoundLibrary is loaded
/// once at startup and never mutated afterwards, both hold in practice.
class SoundPlayer
{
public:
    explicit SoundPlayer(const Assets::SoundLibrary& library);
    ~SoundPlayer();

    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;

    void play(const std::string& key);

    /// 0..100. Applies to subsequent plays; already-playing sounds keep
    /// their original volume.
    void setVolume(float volume);

private:
    sf::Sound& acquireSlot(const sf::SoundBuffer& buffer);

    const Assets::SoundLibrary& library_;
    std::vector<std::unique_ptr<sf::Sound>> pool_;
    float volume_ = 90.0f;
};