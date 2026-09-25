#pragma once

#include <string>

struct Config
{
    bool showDemoWindow = false;

    /// Engine binaries for each side. Independent: either side may run a
    /// different build, the same binary (self-play), or no engine at all
    /// (human-vs-human on that side).
    std::string whiteEnginePath = "C:\\Users\\PC\\Desktop\\stockfish\\stockfish-windows-x86-64-universal.exe";
    std::string blackEnginePath = "C:\\Users\\PC\\Desktop\\stockfish\\stockfish-windows-x86-64-universal.exe";
};