#pragma once

#include "ChessClock.hpp"

#include <string>

struct Config
{
    bool showDemoWindow = false;

    /// Engine binaries for each side. Persisted here so the setup modal
    /// can seed from the last-used paths on the next launch.
    std::string whiteEnginePath = "C:\\Users\\PC\\Desktop\\stockfish\\stockfish-windows-x86-64-universal.exe";
    std::string blackEnginePath = "C:\\Users\\PC\\Desktop\\stockfish\\stockfish-windows-x86-64-universal.exe";

    /// Last committed time control. Reused as the modal's default.
    ChessClock::TimeControl timeControl{};
};