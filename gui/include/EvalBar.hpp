#pragma once

namespace Gui
{

    /// Stateful eval bar. Smooths the engine's reported score toward a
    /// visible fraction so the bar doesn't jump every time a new depth
    /// arrives.
    ///
    /// WhitePct is the fraction of the bar filled with White's color, in
    /// [0, 1]. 0.5 is a dead-even position. It approaches `target` over
    /// time via exponential smoothing; the smoothing is frame-rate
    /// independent so the animation speed is the same at 30 and 240 fps.
    struct EvalBar
    {
        float whitePct = 0.5f; // current, animated
        float target = 0.5f;   // where the bar wants to be

        /// Called once per frame with the latest score, then update().
        /// Pass cp in centipawns from White's POV. Pass mateIn as the
        /// signed mate distance, or 0 if the score is not a mate.
        void setTarget(int cp, int mateIn);

        /// Advances the animation. dtSeconds is the frame delta.
        void update(double dtSeconds);
    };

} // namespace Gui