#pragma once

#include "game/ChessClock.hpp"

#include <string>

struct Config
{
    bool showDemoWindow = false;

    /// Engine binaries for each side. Persisted here so the setup modal
    /// can seed from the last-used paths on the next launch.
    std::string whiteEnginePath = "CheckSpeare.exe";
    std::string blackEnginePath = "CheckSpeare.exe";

    /// Last committed time control. Reused as the modal's default.
    ChessClock::TimeControl timeControl{};
};
