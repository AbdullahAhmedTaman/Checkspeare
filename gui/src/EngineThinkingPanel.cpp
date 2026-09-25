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

        const float barWidth = 40.0f;
        const float barHeight = 480.0f;
        const float lineWidth = 3.0f;
        const float gap = 10.0f;

        const std::string scoreText = formatScore(snapshot);

        // Info block
        ImGui::Text("%s", snapshot.activeSide.c_str());

        if (!snapshot.engineName.empty())
            ImGui::Text("%s", snapshot.engineName.c_str());

        if (snapshot.thinking)
            ImGui::TextColored(ImVec4(0.91f, 0.78f, 0.48f, 1.00f), "Thinking...");
        else
            ImGui::TextDisabled("Idle");

        ImGui::Separator();

        ImGui::Text("Depth: %d / %d", snapshot.depth, snapshot.seldepth);
        ImGui::Text("Nodes: %s", formatCount(snapshot.nodes).c_str());
        ImGui::Text("NPS:   %s", formatCount(snapshot.nps).c_str());

        ImGui::Spacing();

        // Score + eval bar, right-aligned, below the info block
        //
        // Everything on this row is drawn via the draw list rather than
        // through ImGui::Text, because the score must sit at the bar's
        // vertical center rather than at the top of the row. Cursor
        // positioning can only offset from the baseline, not center text
        // against an arbitrary rectangle.
        const float scoreWidth = ImGui::CalcTextSize(scoreText.c_str()).x;
        const float groupWidth = scoreWidth + gap + barWidth;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const float groupLeftOffset = availWidth - groupWidth;

        const ImVec2 rowOrigin = ImGui::GetCursorScreenPos();
        const float groupLeft = rowOrigin.x + (groupLeftOffset > 0.0f ? groupLeftOffset : 0.0f);

        const ImVec2 barP0(groupLeft + scoreWidth + gap, rowOrigin.y);
        const ImVec2 barP1(barP0.x + barWidth, barP0.y + barHeight);

        // Vertical center of the bar, minus half a text line, gives the
        // top-left where AddText expects the glyph block.
        const float textLineH = ImGui::GetTextLineHeight();
        const ImVec2 scorePos(groupLeft, barP0.y + (barHeight - textLineH) * 0.5f);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Black background (whole bar).
        dl->AddRectFilled(barP0, barP1, IM_COL32(30, 29, 28, 255), 4.0f);

        // White fill from the bottom.
        const float whiteH = barHeight * evalBar.whitePct;
        ImVec2 wp0 = ImVec2(barP0.x, barP1.y - whiteH);
        dl->AddRectFilled(wp0, barP1, IM_COL32(242, 241, 240, 255), 4.0f);

        // Center line so the "even" position is visible.
        const float midY = barP0.y + barHeight * 0.5f;
        dl->AddLine(ImVec2(barP0.x, midY), ImVec2(barP1.x, midY), IM_COL32(168, 167, 166, 120), 1.0f);

        // Border.
        dl->AddRect(barP0, barP1, IM_COL32(232, 200, 122, 200), 4.0f, 0, lineWidth);

        // Score text, vertically centered against the bar.
        dl->AddText(scorePos, ImGui::GetColorU32(ImGuiCol_Text), scoreText.c_str());

        // Reserve the row's vertical space so the next widget starts below
        // the bar. Width is the full content width to keep any future
        // horizontal scrolling from treating this row as narrow.
        ImGui::Dummy(ImVec2(availWidth, barHeight));

        ImGui::End();
    }

} // namespace Gui