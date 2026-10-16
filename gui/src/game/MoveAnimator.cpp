#include "game/MoveAnimator.hpp"

#include "game/ChessGame.hpp"

void MoveAnimator::update(const ChessGame& game, std::size_t cursor, bool lastMoveWasDrag, float dtSeconds)
{
    // A single forward step is a real commit; a single backward step is
    // an undo. Multi-ply jumps snap.
    if (cursor == lastCursor_ + 1)
    {
        if (!lastMoveWasDrag)
        {
            const MoveRecord& rec = game.history()[cursor - 1];
            animation_.start(rec.move.from(), rec.move.to(), game.pieceAt(rec.move.to()));
        }
    }
    else if (cursor + 1 == lastCursor_ && cursor < game.history().size())
    {
        const MoveRecord& rec = game.history()[cursor];
        animation_.start(rec.move.to(), rec.move.from(), game.pieceAt(rec.move.from()));
    }
    else if (cursor != lastCursor_)
    {
        animation_.active = false;
    }

    animation_.tick(dtSeconds);
    lastCursor_ = cursor;
}

void MoveAnimator::reset(std::size_t cursor)
{
    animation_.active = false;
    lastCursor_ = cursor;
}