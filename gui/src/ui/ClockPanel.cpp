// ClockPanel.cpp
#include "ClockPanel.hpp"

#include "CapturedPiecesPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <string>

namespace Gui
{

    namespace
    {
        /// Formats a millisecond count as mm:ss. Negative values clamp to 0.
        std::string formatClock(std::int64_t ms)
        {
            if (ms < 0)
                ms = 0;
            const std::int64_t totalSeconds = ms / 1000;
            const std::int64_t minutes = totalSeconds / 60;
            const std::int64_t seconds = totalSeconds % 60;

            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02lld:%02lld", static_cast<long long>(minutes), static_cast<long long>(seconds));
            return buf;
        }

        struct CardColors
        {
            ImU32 bg;
            ImU32 border;
            ImU32 accent;
            ImU32 text;
            ImU32 barBg;
            ImU32 barFill;
        };

        CardColors colorsFor(bool active, bool flagged)
        {
            CardColors c;

            if (flagged)
            {
                c.bg = IM_COL32(56, 26, 24, 255);
                c.border = IM_COL32(198, 96, 78, 255);
                c.accent = IM_COL32(214, 112, 90, 255);
                c.text = IM_COL32(242, 218, 208, 255);
                c.barBg = IM_COL32(36, 16, 14, 255);
                c.barFill = IM_COL32(176, 82, 66, 255);
            }
            else if (active)
            {
                c.bg = IM_COL32(46, 42, 38, 255);
                c.border = IM_COL32(232, 200, 122, 255);
                c.accent = IM_COL32(232, 200, 122, 255);
                c.text = IM_COL32(242, 235, 221, 255);
                c.barBg = IM_COL32(30, 27, 24, 255);
                c.barFill = IM_COL32(232, 200, 122, 255);
            }
            else
            {
                c.bg = IM_COL32(36, 35, 34, 255);
                c.border = IM_COL32(74, 73, 72, 255);
                c.accent = IM_COL32(140, 139, 138, 255);
                c.text = IM_COL32(184, 183, 182, 255);
                c.barBg = IM_COL32(26, 25, 25, 255);
                c.barFill = IM_COL32(122, 121, 120, 255);
            }

            return c;
        }

        /// Draws a single clock card at the current cursor position.
        void drawClockCard(const char* label, const std::string& engineName, std::int64_t remainingMs, std::int64_t referenceMs,
                           bool active, bool flagged, ImVec2 size, ImFont* clockFont, ImFont* engineNameFont)
        {
            const CardColors c = colorsFor(active, flagged);

            ImGui::PushID(label);

            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

            ImDrawList* dl = ImGui::GetWindowDrawList();

            const float borderThickness = active ? 2.5f : 1.0f;
            const float rounding = 8.0f;

            dl->AddRectFilled(p0, p1, c.border, rounding);

            const ImVec2 inner0 = ImVec2(p0.x + borderThickness, p0.y + borderThickness);
            const ImVec2 inner1 = ImVec2(p1.x - borderThickness, p1.y - borderThickness);
            dl->AddRectFilled(inner0, inner1, c.bg, rounding - borderThickness);

            const float pad = 12.0f;
            const float dotRadius = 4.5f;
            const float dotGap = 10.0f;
            const float rowY = p0.y + pad;
            const float lineH = ImGui::GetTextLineHeight();

            const ImVec2 dotCenter(p0.x + pad + dotRadius, rowY + lineH * 0.5f);
            dl->AddCircleFilled(dotCenter, dotRadius, c.accent);

            const float labelX = p0.x + pad + dotRadius * 2.0f + dotGap;
            dl->AddText(ImVec2(labelX, rowY), c.text, label);

            if (!engineName.empty())
            {
                ImFont* font = engineNameFont ? engineNameFont : ImGui::GetFont();
                const float nameSize = engineNameFont ? engineNameFont->FontSize : ImGui::GetFontSize();

                const float nameW = font->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, engineName.c_str()).x;
                const float nameX = p0.x + size.x - pad - nameW;
                const float nameLineH = nameSize;
                const float labelLineH = lineH;
                const float nameY = rowY + (labelLineH - nameLineH) * 0.5f;

                const float labelW = ImGui::CalcTextSize(label).x;
                if (nameX > labelX + labelW + 8.0f)
                    dl->AddText(font, nameSize, ImVec2(nameX, nameY), c.accent, engineName.c_str());
            }

            const std::string timeText = formatClock(remainingMs);

            ImFont* font = clockFont ? clockFont : ImGui::GetFont();
            const float bigSize = clockFont ? clockFont->FontSize : 34.0f;

            const ImVec2 timeSize = font->CalcTextSizeA(bigSize, FLT_MAX, 0.0f, timeText.c_str());

            const ImVec2 timePos = ImVec2(p0.x + (size.x - timeSize.x) * 0.5f, p0.y + pad + 26.0f);
            dl->AddText(font, bigSize, timePos, c.text, timeText.c_str());

            const float barH = 6.0f;
            const float barY = p1.y - pad - barH;
            const float barW = size.x - pad * 2.0f;
            const ImVec2 barP0 = ImVec2(p0.x + pad, barY);
            const ImVec2 barP1 = ImVec2(barP0.x + barW, barY + barH);

            dl->AddRectFilled(barP0, barP1, c.barBg, 3.0f);

            const float frac = (referenceMs > 0) ? static_cast<float>(remainingMs) / static_cast<float>(referenceMs) : 0.0f;
            const float clamped = std::clamp(frac, 0.0f, 1.0f);

            if (clamped > 0.0f)
            {
                const ImVec2 fillP1 = ImVec2(barP0.x + barW * clamped, barP1.y);
                dl->AddRectFilled(barP0, fillP1, c.barFill, 3.0f);
            }

            ImGui::Dummy(size);

            ImGui::PopID();
        }

        /// Draws one row of the pieces that `capturingSide` has captured.
        /// Each distinct piece type is rendered once with its count.
        void drawCapturedRow(const ChessGame& game, Color capturingSide, const Assets::TextureLibrary& assets, float iconSize)
        {
            constexpr float kGroupGap = 8.0f;

            const Color capturedSide = (capturingSide == WHITE) ? BLACK : WHITE;
            const std::vector<CapturedGroup> groups = capturedBy(game, capturingSide);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float availW = ImGui::GetContentRegionAvail().x;

            if (groups.empty())
            {
                // Still reserve the row height so both sides keep matching layout.
                ImGui::Dummy(ImVec2(availW, iconSize));
                return;
            }

            // Resolve each group's texture once so both passes agree.
            std::vector<std::pair<const sf::Texture*, int>> resolved;
            resolved.reserve(groups.size());
            for (const CapturedGroup& g : groups)
            {
                const std::string key = Assets::TextureLibrary::getPieceKey(make_piece(capturedSide, g.type));
                resolved.emplace_back(&assets.getTexture(key), g.count);
            }

            // Measure pass. drawCapturedIcon always draws, so the first pass is
            // issued far off-screen; the child's clip rect discards its pixels.
            // (A measure-only entry point would be cleaner, but this avoids
            // touching CapturedPiecesPanel.hpp.)
            float totalW = 0.0f;
            {
                const ImVec2 probe(-100000.0f, -100000.0f);
                for (const auto& [texture, count] : resolved)
                    totalW += drawCapturedIcon(drawList, *texture, probe, iconSize, count) + kGroupGap;
                totalW -= kGroupGap;
            }

            // Draw pass, centered horizontally inside the child.
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            if (availW > totalW)
                cursor.x += (availW - totalW) * 0.5f;

            for (const auto& [texture, count] : resolved)
                cursor.x += drawCapturedIcon(drawList, *texture, cursor, iconSize, count) + kGroupGap;

            ImGui::Dummy(ImVec2(availW, iconSize));
        }
    } // namespace

    void renderClockPanel(const ChessClock& clock, const ChessGame& game, const Assets::TextureLibrary& assets, ImFont* clockFont,
                          ImFont* engineNameFont, const std::string& whiteEngineName, const std::string& blackEngineName)
    {
        ImGui::Begin("Clock");

        const float availW = ImGui::GetContentRegionAvail().x;
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float cardW = (availW - gap) * 0.5f;
        const float cardH = 100.0f;

        const std::int64_t reference = clock.timeControl().initialMs;
        const bool whiteToMove = (game.sideToMove() == WHITE);

        drawClockCard("White", whiteEngineName, clock.remainingMs(WHITE), reference, whiteToMove, clock.flagFallen(WHITE),
                      ImVec2(cardW, cardH), clockFont, engineNameFont);

        ImGui::SameLine();

        drawClockCard("Black", blackEngineName, clock.remainingMs(BLACK), reference, !whiteToMove, clock.flagFallen(BLACK),
                      ImVec2(cardW, cardH), clockFont, engineNameFont);

        // Captured pieces sit beneath each player's clock: White's row of
        // captured black pieces under the White clock, and Black's row of
        // captured white pieces under the Black clock. Wrapping each row in
        // a borderless, non-scrolling child gives clean clipping if a side
        // ever accumulates more pieces than the width allows.
        constexpr float kIconSize = 34.0f;
        const float rowH = kIconSize + 8.0f;

        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 2.0f));

        ImGui::BeginChild("##whiteCaptures", ImVec2(cardW, rowH), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        drawCapturedRow(game, WHITE, assets, kIconSize);
        ImGui::EndChild();

        ImGui::SameLine(0.0f, gap);

        ImGui::BeginChild("##blackCaptures", ImVec2(cardW, rowH), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        drawCapturedRow(game, BLACK, assets, kIconSize);
        ImGui::EndChild();

        ImGui::PopStyleVar();

        ImGui::End();
    }

} // namespace Gui