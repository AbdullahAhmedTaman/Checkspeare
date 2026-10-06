#include "SoundPlayer.hpp"
#include <SFML/Audio/Sound.hpp>

SoundPlayer::SoundPlayer(const Assets::SoundLibrary& library)
    : library_(library)
{
}

SoundPlayer::~SoundPlayer() = default;

void SoundPlayer::play(const std::string& key)
{
    const sf::SoundBuffer& buffer = library_.randomBuffer(key);

    sf::Sound& slot = acquireSlot(buffer);
    slot.setVolume(volume_);
    slot.play();
}

sf::Sound& SoundPlayer::acquireSlot(const sf::SoundBuffer& buffer)
{
    for (auto& slot : pool_)
    {
        if (slot->getStatus() != sf::SoundSource::Status::Playing)
        {
            slot->setBuffer(buffer);
            return *slot;
        }
    }

    // All slots busy; grow the pool. The new slot is constructed with a
    // buffer because SFML 3 has no default constructor.
    pool_.push_back(std::make_unique<sf::Sound>(buffer));
    return *pool_.back();
}