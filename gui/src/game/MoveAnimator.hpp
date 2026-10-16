#pragma once

#include "game/MoveAnimation.hpp"

#include <cstddef>

class ChessGame;

/// Drives the piece-slide animation from cursor changes. Tracks the
/// previously observed cursor to decide whether the current one is a
/// commit, an undo, or a jump.
class MoveAnimator
{
public:
    /// Advances the animation for this frame. `lastMoveWasDrag` suppresses
    /// the slide for a commit that came from a drag — the dragged piece is
    /// already under the cursor.
    void update(const ChessGame& game, std::size_t cursor, bool lastMoveWasDrag, float dtSeconds);

    /// Clears any in-flight animation and adopts `cursor` as the new
    /// baseline. Called when the position is reset or replaced.
    void reset(std::size_t cursor);

    const Gui::MoveAnimation& animation() const
    {
        return animation_;
    }

private:
    Gui::MoveAnimation animation_;
    std::size_t lastCursor_ = 0;
};