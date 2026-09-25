#include "EvalBar.hpp"

#include <cmath>

namespace Gui
{

    namespace
    {
        // Score-to-fraction curve. Linear maps 0..+5 pawns to 0.5..1.0,
        // but that makes +0.5 barely visible and +2 already almost full.
        // A sigmoid gives the visual weight a human expects: most of the
        // bar is used for the interesting range (-2 to +2 pawns).
        //
        // This is the same curve Lichess uses. Tuned by eye, not theory.
        float scoreToFraction(int cp)
        {
            const double k = 0.004; // ~half a pawn per 10% bar
            const double x = static_cast<double>(cp) * k;
            const double sigmoid = 1.0 / (1.0 + std::exp(-x));
            return static_cast<float>(sigmoid);
        }

        // Exponential smoothing rate. Higher = snappier, lower = smoother.
        // 6.0 gives ~0.3s to close 90% of the gap.
        constexpr double kLerpRate = 6.0;
    } // namespace

    void EvalBar::setTarget(int cp, int mateIn)
    {
        if (mateIn > 0)
            target = 1.0f; // White mates
        else if (mateIn < 0)
            target = 0.0f; // Black mates
        else
            target = scoreToFraction(cp);
    }

    void EvalBar::update(double dtSeconds)
    {
        if (dtSeconds <= 0.0)
            return;

        // Frame-rate independent lerp. `alpha` is the fraction of the
        // remaining gap to close this frame. Approaching asymptotically,
        // never overshooting, stable for any dt.
        const double alpha = 1.0 - std::exp(-kLerpRate * dtSeconds);
        whitePct += static_cast<float>((target - whitePct) * alpha);
    }

} // namespace Gui