#include "ClockPanel.hpp"

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
                // Red palette: the flag fell.
                c.bg = IM_COL32(58, 22, 22, 255);
                c.border = IM_COL32(210, 70, 70, 255);
                c.accent = IM_COL32(230, 90, 90, 255);
                c.text = IM_COL32(245, 205, 205, 255);
                c.barBg = IM_COL32(38, 14, 14, 255);
                c.barFill = IM_COL32(190, 65, 65, 255);
            }
            else if (active)
            {
                // Amber palette: this side is to move.
                c.bg = IM_COL32(38, 42, 52, 255);
                c.border = IM_COL32(240, 184, 76, 255);
                c.accent = IM_COL32(240, 184, 76, 255);
                c.text = IM_COL32(252, 247, 236, 255);
                c.barBg = IM_COL32(28, 32, 40, 255);
                c.barFill = IM_COL32(240, 184, 76, 255);
            }
            else
            {
                // Neutral palette: waiting.
                c.bg = IM_COL32(28, 32, 40, 255);
                c.border = IM_COL32(58, 64, 78, 255);
                c.accent = IM_COL32(120, 130, 150, 255);
                c.text = IM_COL32(178, 184, 198, 255);
                c.barBg = IM_COL32(22, 26, 34, 255);
                c.barFill = IM_COL32(100, 110, 130, 255);
            }

            return c;
        }

        /// Draws a single clock card at the current cursor position.
        ///
        /// The card is drawn with the window draw list (background, border,
        /// dot, big time, progress bar), then ImGui::Dummy reserves the
        /// layout slot so the next widget lands after the card.

        /// Draws a single clock card at the current cursor position.
        ///
        /// The card is drawn with the window draw list (background, border,
        /// dot, big time, progress bar), then ImGui::Dummy reserves the
        /// layout slot so the next widget lands after the card.
        ///
        /// `clockFont` may be null; if so, the time is drawn with the
        /// default UI font at a larger point size.
        void drawClockCard(const char* label, std::int64_t remainingMs, std::int64_t referenceMs, bool active, bool flagged,
                           ImVec2 size, ImFont* clockFont)
        {
            const CardColors c = colorsFor(active, flagged);

            ImGui::PushID(label);

            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Card background + border.
            //
            // Drawn as two nested *filled* rects rather than AddRect with a
            // thickness. A stroked rounded rect puts two AA fringes close
            // together at 2px thickness and the corners come out pinched;
            // an outer fill plus an inset inner fill has no such fringe.
            const float borderThickness = active ? 2.5f : 1.0f;
            const float rounding = 8.0f;

            dl->AddRectFilled(p0, p1, c.border, rounding);

            // Inset the inner fill by the border thickness and reduce the
            // corner radius by the same amount so the two curves stay
            // concentric.
            const ImVec2 inner0 = ImVec2(p0.x + borderThickness, p0.y + borderThickness);
            const ImVec2 inner1 = ImVec2(p1.x - borderThickness, p1.y - borderThickness);
            dl->AddRectFilled(inner0, inner1, c.bg, rounding - borderThickness);

            const float pad = 12.0f;

            // Active dot + label, top-left.
            const ImVec2 dotCenter = ImVec2(p0.x + pad + 5.0f, p0.y + pad + 7.0f);
            dl->AddCircleFilled(dotCenter, 4.5f, c.accent);
            dl->AddText(ImVec2(dotCenter.x + 12.0f, p0.y + pad), c.text, label);

            // Big time readout, horizontally centered.
            //
            // If a clock font was supplied, use it at its native size (the
            // size it was rasterized at). Otherwise fall back to the default
            // font scaled up, which is legible but softer.
            const std::string timeText = formatClock(remainingMs);

            ImFont* font = clockFont ? clockFont : ImGui::GetFont();
            const float bigSize = clockFont ? clockFont->FontSize : 34.0f;

            const ImVec2 timeSize = font->CalcTextSizeA(bigSize, FLT_MAX, 0.0f, timeText.c_str());

            const ImVec2 timePos = ImVec2(p0.x + (size.x - timeSize.x) * 0.5f, p0.y + pad + 26.0f);
            dl->AddText(font, bigSize, timePos, c.text, timeText.c_str());

            // Progress bar: remaining / reference, clamped to [0, 1].
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

            // Reserve the layout slot now that the card has been drawn.
            ImGui::Dummy(size);

            ImGui::PopID();
        }
    } // namespace

    void renderClockPanel(const ChessClock& clock, const ChessGame& game, ImFont* clockFont)
    {
        ImGui::Begin("Clock");

        const float availW = ImGui::GetContentRegionAvail().x;
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float cardW = (availW - gap) * 0.5f;
        const float cardH = 100.0f;

        // The clock does not expose its initial time, so the bar is drawn
        // relative to whichever side has the most time left. The leader is
        // at 100%, the other side at whatever fraction remains.
        const std::int64_t reference = std::max(clock.remainingMs(WHITE), clock.remainingMs(BLACK));

        const bool whiteToMove = (game.sideToMove() == WHITE);

        drawClockCard("White", clock.remainingMs(WHITE), reference, whiteToMove, clock.flagFallen(WHITE), ImVec2(cardW, cardH),
                      clockFont);

        ImGui::SameLine();

        drawClockCard("Black", clock.remainingMs(BLACK), reference, !whiteToMove, clock.flagFallen(BLACK), ImVec2(cardW, cardH),
                      clockFont);

        ImGui::End();
    }

} // namespace Gui