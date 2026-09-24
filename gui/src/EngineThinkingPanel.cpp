#include "EngineThinkingPanel.hpp"

#include <imgui.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace Gui
{

    namespace
    {
        /// Formats the engine's score for display.
        /// Prefers mate scores over centipawn scores when both are set,
        /// because UCI engines send mate as a truncation of cp.
        std::string formatScore(const EngineThinkingSnapshot& s)
        {
            if (s.scoreMate)
            {
                const int n = *s.scoreMate;
                if (n > 0)
                    return "#" + std::to_string(n);
                if (n < 0)
                    return "#-" + std::to_string(-n);
                return "#0"; // engine reported mate-in-0; unusual
            }

            if (s.scoreCp)
            {
                const int cp = *s.scoreCp;
                const double pawns = cp / 100.0;
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%+.2f", pawns);
                return buf;
            }

            return "-";
        }

        std::string formatCount(std::int64_t n)
        {
            if (n >= 1'000'000'000)
                return std::to_string(n / 1'000'000'000) + "G";
            if (n >= 1'000'000)
                return std::to_string(n / 1'000'000) + "M";
            if (n >= 1'000)
                return std::to_string(n / 1'000) + "K";
            return std::to_string(n);
        }
    } // namespace

    void renderEngineThinkingPanel(const EngineThinkingSnapshot& snapshot, const EvalBar& evalBar)
    {
        ImGui::Begin("Engine");

        ImGui::Text("Side: %s", snapshot.activeSide.c_str());

        if (snapshot.thinking)
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Thinking...");
        else
            ImGui::TextDisabled("Idle");

        ImGui::Separator();

        const std::string score = formatScore(snapshot);

        ImGui::Text("Score: %s", score.c_str());
        ImGui::Text("Depth: %d / %d", snapshot.depth, snapshot.seldepth);
        ImGui::Text("Nodes: %s", formatCount(snapshot.nodes).c_str());
        ImGui::Text("NPS:   %s", formatCount(snapshot.nps).c_str());

        // PV intentionally omitted for MVP. Rendering it requires replaying
        // each UCI move against a copy of the current Board to produce SAN.
        const float barWidth = 30.0f;
        const float barHeight = 240.0f;
        const float lineWidth = 3.0f;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + barWidth, p0.y + barHeight);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Black background (whole bar).
        dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255), 4.0f);

        // White fill from the bottom.
        const float whiteH = barHeight * evalBar.whitePct;
        ImVec2 wp0 = ImVec2(p0.x, p1.y - whiteH);
        dl->AddRectFilled(wp0, p1, IM_COL32(240, 240, 240, 255), 4.0f);

        // Center line so the "even" position is visible.
        const float midY = p0.y + barHeight * 0.5f;
        dl->AddLine(ImVec2(p0.x, midY), ImVec2(p1.x, midY), IM_COL32(180, 180, 180, 120), 1.0f);

        // Border.
        dl->AddRect(p0, p1, IM_COL32(200, 200, 200, 200), 4.0f, 0, lineWidth);

        // Reserve layout space so ImGui doesn't overlap the next widget.
        ImGui::Dummy(ImVec2(barWidth, barHeight));

        // Score text next to the bar.
        ImGui::SameLine();
        ImGui::Text("%s", formatScore(snapshot).c_str());

        ImGui::End();
    }

} // namespace Gui