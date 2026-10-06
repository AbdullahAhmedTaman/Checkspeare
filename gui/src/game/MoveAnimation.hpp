#pragma once

#include "board/types.h"

namespace Gui
{

    /// Visual slide of a piece between two squares. The underlying game
    /// state already reflects the move; the renderer uses this only to
    /// interpolate the drawn position for a short interval.
    struct MoveAnimation
    {
        bool active = false;
        Square from = SQ_NONE;
        Square to = SQ_NONE;
        Piece piece = PIECE_NONE;
        float elapsed = 0.0f;
        float duration = 0.22f;

        /// `movedPiece` is the piece at `toSq` in the post-move position,
        /// so a promotion animates as the promoted piece.
        void start(Square fromSq, Square toSq, Piece movedPiece)
        {
            active = true;
            from = fromSq;
            to = toSq;
            piece = movedPiece;
            elapsed = 0.0f;
        }

        /// Returns true while active after advancing the timer.
        bool tick(float dtSeconds)
        {
            if (!active)
                return false;

            elapsed += dtSeconds;
            if (elapsed >= duration)
                active = false;

            return active;
        }

        /// Interpolation parameter in [0, 1] with ease-out cubic applied.
        /// Returns 1 when inactive so callers can lerp unconditionally.
        float easedT() const
        {
            if (!active)
                return 1.0f;

            float t = elapsed / duration;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

            const float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }
    };

} // namespace Gui