#pragma once

#include "game/ChessClock.hpp"

#include <string>

#ifdef _WIN32
constexpr const char* DEFAULT_ENGINE_PATH = "CheckspeareEngine.exe";
#else
constexpr const char* DEFAULT_ENGINE_PATH = "CheckspeareEngine";
#endif

struct Config
{
    bool showDemoWindow = false;

    /// Engine binaries for each side. Persisted here so the setup modal
    /// can seed from the last-used paths on the next launch.
    std::string whiteEnginePath = DEFAULT_ENGINE_PATH;
    std::string blackEnginePath = DEFAULT_ENGINE_PATH;

    /// Last committed time control. Reused as the modal's default.
    ChessClock::TimeControl timeControl{};
};