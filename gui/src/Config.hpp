#pragma once

#include "ResourcePaths.hpp"
#include "game/ChessClock.hpp"

#include <string>

#ifdef _WIN32
constexpr const char* ENGINE_FILE_NAME = "CheckspeareEngine.exe";
#else
constexpr const char* ENGINE_FILE_NAME = "CheckspeareEngine";
#endif

/// The engine binary shipped next to the GUI executable.
inline std::string defaultEnginePath()
{
    return (executableDir() / ENGINE_FILE_NAME).string();
}

struct Config
{
    bool showDemoWindow = false;

    /// Engine binaries for each side. Persisted here so the setup modal
    /// can seed from the last-used paths on the next launch.
    std::string whiteEnginePath = defaultEnginePath();
    std::string blackEnginePath = defaultEnginePath();

    /// Last committed time control. Reused as the modal's default.
    ChessClock::TimeControl timeControl{};
};