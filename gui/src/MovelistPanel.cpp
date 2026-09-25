#include "MoveListPanel.hpp"
#include "ChessGame.hpp"
#include "GuiGameController.hpp"

#include <imgui.h>

#include <cstddef>

namespace Gui
{

    namespace
    {
        /// Immutable copy of one ply, used so the table rows don't hold
        /// references into history() while iterating.
        struct PlyView
        {
            std::string san;
        };
    } // namespace

    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller, ImFont* moveListFont)
    {
        ImGui::Begin("Moves");

        // Fixed footer: buttons at the bottom, list scrolls above.
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::BeginChild("##movelist", ImVec2(0, avail.y - footerHeight), true);

        const auto records = game.historyToCursor();
        const std::size_t cursor = game.cursor();

        // 2 columns: move number | white san | black san.
        //
        // SizingStretchSame gives the two SAN columns equal shares of the
        // panel's width instead of hugging their content. That is what
        // lets White's move sit at the left edge of the panel and Black's
        // at the right edge, with the move number bridging them.
        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody;

        // ImGui's table cells get a CellPadding on each side by default,
        // which on top of the fixed number column pushes the first SAN
        // move away from the panel's left edge. Zero it so the number and
        // White's move sit flush left, and the same on the right so
        // Black's right-aligned move hugs the panel edge.
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 4.0f));

        if (ImGui::BeginTable("##pliestable", 3, flags))
        {
            // Narrow number column, sized to hold "100." at the move-list
            // font without clipping. The table's own stretch logic gives
            // the rest of the width to the two SAN columns.
            ImGui::TableSetupColumn("##num", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableSetupColumn("##white", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##black", ImGuiTableColumnFlags_WidthStretch);

            // The whole table body uses the move-list font so the numbers
            // and the SAN text share a typeface.
            if (moveListFont != nullptr)
                ImGui::PushFont(moveListFont);

            for (std::size_t i = 0; i < records.size(); i += 2)
            {
                ImGui::TableNextRow();

                // Move number
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu.", i / 2 + 1);

                // White's move (ply i) and Black's move (ply i+1)
                for (int col = 1; col <= 2; ++col)
                {
                    const std::size_t ply = i + col - 1;
                    ImGui::TableSetColumnIndex(col);

                    if (ply >= records.size())
                        continue;

                    const bool isCurrent = (ply + 1 == cursor);
                    const bool isPastCursor = (ply >= cursor);

                    if (isCurrent)
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 60, 255));
                    else if (isPastCursor)
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 160, 160, 255));

                    // Black's move is right-aligned within its column so it
                    // anchors to the right edge of the panel; White's stays
                    // left-anchored in its column. The cursor offset has to
                    // be applied before Selectable draws, since Selectable
                    // paints the row background from its current cursor
                    // position.
                    if (col == 2)
                    {
                        const float textW = ImGui::CalcTextSize(records[ply].san.c_str()).x;
                        const float availW = ImGui::GetContentRegionAvail().x;
                        if (availW > textW)
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - textW));
                    }

                    ImGui::PushID(static_cast<int>(ply));
                    if (ImGui::Selectable(records[ply].san.c_str(), isCurrent, ImGuiSelectableFlags_AllowOverlap))
                    {
                        // Jump the cursor to just after this ply.
                        controller.requestGotoPly(ply + 1);
                    }
                    ImGui::PopID();

                    if (isCurrent || isPastCursor)
                        ImGui::PopStyleColor();
                }
            }

            if (moveListFont != nullptr)
                ImGui::PopFont();

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        // Auto-scroll to the current row when it changes.
        // (Simple approach: if the user is at the end, keep the list pinned
        //  to the bottom.)
        if (cursor > 0 && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();

        // ---- Footer: Undo / Redo ----
        const bool canUndo = game.cursor() > 0;
        const bool canRedo = game.cursor() < game.totalMoves();

        ImGui::BeginDisabled(!canUndo);
        if (ImGui::Button("Undo"))
            controller.requestUndo();
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!canRedo);
        if (ImGui::Button("Redo"))
            controller.requestRedo();
        ImGui::EndDisabled();

        ImGui::End();
    }

} // namespace Gui