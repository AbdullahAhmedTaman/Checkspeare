#pragma once

#include <cstdint>
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
        int scoreCp = 0;
        int scoreMate = 0;
        std::vector<std::string> pvSan;
        std::string activeSide;
    };

    /// @note Not yet implemented.
    void renderEngineThinkingPanel(const EngineThinkingSnapshot& snapshot);

} // namespace Gui