#pragma once

#include "EvalBar.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Gui
{

    /// Snapshot of the engine's most recent search, used by the "Engine
    /// Thinking" panel. Producer and consumer depend on this struct only,
    /// not on UciEngine or EnginePlayer.
    struct EngineThinkingSnapshot
    {
        bool thinking = false;
        int depth = 0;
        int seldepth = 0;
        std::int64_t nodes = 0;
        std::int64_t nps = 0;
        std::optional<int> scoreCp;
        std::optional<int> scoreMate;
        std::vector<std::string> pvSan;
        std::string activeSide;
    };

    void renderEngineThinkingPanel(const EngineThinkingSnapshot& snapshot, const EvalBar& evalBar);
} // namespace Gui