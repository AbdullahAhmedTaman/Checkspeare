#pragma once

/// Returns true exactly once per transition into game-over.
///
/// `wasOver` is the caller's remembered state from the previous frame.
/// The function updates it in place, so the caller's flag reflects the
/// current frame after the call.
///
/// Behaviour table:
///     isOver  wasOver   returns   wasOver after
///     ------  -------   -------   ------------
///     false   false     false     false
///     true    false     true      true      <- fires once on entry
///     true    true      false     true      <- does not re-fire
///     false   true      false     false     <- reset when game restarts
///     true    false     true      true      <- fires again on a new game's end
inline bool justEnteredGameOver(bool isOver, bool& wasOver)
{
    const bool entered = isOver && !wasOver;
    wasOver = isOver;
    return entered;
}