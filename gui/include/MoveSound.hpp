#pragma once

/// Sound category derived from a committed move. Populated by ChessGame at
/// commit time so playback code does not have to re-inspect the position.
enum class MoveSound
{
    QuietMove,
    Capture,
    Check,
    Castle,
    Promotion,
};