#include "audio/SoundDirector.hpp"

#include "game/GameResult.hpp"

namespace
{
    const char* soundKeyFor(MoveSound s)
    {
        switch (s)
        {
            case MoveSound::QuietMove:
                return Assets::SoundKey::Move;
            case MoveSound::Capture:
                return Assets::SoundKey::Capture;
            case MoveSound::Check:
                return Assets::SoundKey::Check;
            case MoveSound::Castle:
                return Assets::SoundKey::Castle;
            case MoveSound::Promotion:
                return Assets::SoundKey::Promote;
        }
        return Assets::SoundKey::Move;
    }
} // namespace

SoundDirector::SoundDirector(SoundPlayer& player)
    : player_(player)
{
}

void SoundDirector::setSideIsHuman(std::function<bool(Color)> pred)
{
    sideIsHuman_ = std::move(pred);
}

void SoundDirector::playMoveSound(MoveSound s)
{
    player_.play(soundKeyFor(s));
}

std::string SoundDirector::gameOverKey(const ChessGame& game, const ChessClock& clock) const
{
    // Time loss beats anything the board says: the flag decides the game.
    const auto outcomeFor = [this](Color loser)
    {
        const Color winner = (loser == WHITE) ? BLACK : WHITE;
        const bool loserHuman = sideIsHuman_(loser);
        const bool winnerHuman = sideIsHuman_(winner);

        if (loserHuman && !winnerHuman)
            return std::string(Assets::SoundKey::Defeat);
        if (winnerHuman && !loserHuman)
            return std::string(Assets::SoundKey::Victory);

        // Both sides human (hotseat) or both engines (spectating): play
        // the victory cue for whoever won.
        return std::string(Assets::SoundKey::Victory);
    };

    if (clock.flagFallen(WHITE))
        return outcomeFor(WHITE);
    if (clock.flagFallen(BLACK))
        return outcomeFor(BLACK);

    const GameResult result = game.result();

    switch (result.kind)
    {
        case GameResultKind::Checkmate:
        case GameResultKind::Resignation:
            {
                const Color loser = (result.winner == WHITE) ? BLACK : WHITE;
                return outcomeFor(loser);
            }

        case GameResultKind::Stalemate:
        case GameResultKind::FiftyMove:
        case GameResultKind::Threefold:
        case GameResultKind::InsufficientMaterial:
            return std::string(Assets::SoundKey::Draw);

        case GameResultKind::Ongoing:
            return {};
    }
    return {};
}

void SoundDirector::playGameOverSound(const ChessGame& game, const ChessClock& clock)
{
    const std::string key = gameOverKey(game, clock);
    if (!key.empty())
        player_.play(key);
}

void SoundDirector::onActionResult(Gui::ActionResult::Kind kind)
{
    if (kind == lastResultKind_)
        return;
    if (kind == Gui::ActionResult::Kind::IllegalMove)
        player_.play(Assets::SoundKey::Illegal);
    lastResultKind_ = kind;
}