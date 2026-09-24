#include "ChessGame.hpp"
#include "GuiGameController.hpp"
#include "MoveListPanel.hpp"

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

    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller, const BoardGeometry& geo)
    {
        constexpr float kPadding = 20.0f;
        const float panelX = geo.boardOrigin.x + geo.boardWidth() + kPadding;
        const float panelY = geo.boardOrigin.y;
        const float panelH = geo.boardHeight();
        const float panelW = 220.0f;

        ImGui::SetNextWindowPos({panelX, panelY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);

        ImGui::Begin("Moves", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoSavedSettings);

        // Fixed footer: buttons at the bottom, list scrolls above.
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::BeginChild("##movelist", ImVec2(0, avail.y - footerHeight), true);

        const auto records = game.historyToCursor();
        const std::size_t cursor = game.cursor();

        // 2 columns: move number | white san | black san.
        // Use a table so widths auto-size and scroll behaves.
        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody;

        if (ImGui::BeginTable("##pliestable", 3, flags))
        {
            ImGui::TableSetupColumn("##num", ImGuiTableColumnFlags_WidthFixed, 32.0f);
            ImGui::TableSetupColumn("##white");
            ImGui::TableSetupColumn("##black");

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

            ImGui::EndTable();
        }

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