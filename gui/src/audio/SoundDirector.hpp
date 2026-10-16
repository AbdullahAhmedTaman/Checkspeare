#pragma once

#include "audio/MoveSound.hpp"
#include "audio/SoundPlayer.hpp"
#include "controller/GuiGameController.hpp"
#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"

#include <functional>
#include <string>

/// Plays the game's auditory cues: committed-move sounds, the
/// game-over outcome, and the illegal-move buzz.
///
/// Outcome cues depend on which sides are human; the caller supplies a
/// predicate. Everything else is derived from the game and clock.
class SoundDirector
{
public:
    explicit SoundDirector(SoundPlayer& player);

    /// Installs the predicate used to pick victory/defeat/draw cues.
    void setSideIsHuman(std::function<bool(Color)> pred);

    /// Plays the sound for a committed move.
    void playMoveSound(MoveSound s);

    /// Plays the outcome sound for the current game-over state. No-op
    /// while the game is ongoing and no clock has flagged.
    void playGameOverSound(const ChessGame& game, const ChessClock& clock);

    /// Plays the illegal-move cue on the frame `kind` first becomes
    /// ActionResult::Kind::IllegalMove. Call once per frame with the
    /// controller's current result kind.
    void onActionResult(Gui::ActionResult::Kind kind);

private:
    std::string gameOverKey(const ChessGame& game, const ChessClock& clock) const;

    SoundPlayer& player_;
    std::function<bool(Color)> sideIsHuman_;
    Gui::ActionResult::Kind lastResultKind_ = Gui::ActionResult::Kind::Ready;
};