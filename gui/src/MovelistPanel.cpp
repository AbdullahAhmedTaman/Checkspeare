// MovelistPanel.cpp
#include "MoveListPanel.hpp"
#include "ChessGame.hpp"
#include "GameResult.hpp"
#include "GuiGameController.hpp"

#include <imgui.h>

#include <cstddef>

namespace Gui
{

    namespace
    {
        struct PlyView
        {
            std::string san;
        };
    } // namespace

    void renderMoveListPanel(const ChessGame& game, GuiGameController& controller)
    {
        ImGui::Begin("Moves");

        // Terminal position: navigation would let the user "unfinish" a
        // finished game. Disable the buttons and the clickable rows.
        const bool gameOver = (game.result().kind != GameResultKind::Ongoing);

        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::BeginChild("##movelist", ImVec2(0, avail.y - footerHeight), true);

        // Scale up the ply list (move numbers + SAN) without touching the
        // footer buttons, which live outside this child window.
        //
        // Note: this stretches the existing font atlas, so glyphs will be
        // slightly soft. A crisp alternative requires a second, larger font
        // loaded into the atlas and selected via PushFont(font).
        ImGui::SetWindowFontScale(1.15f);

        const auto records = game.historyToCursor();
        const std::size_t cursor = game.cursor();

        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoBordersInBody;

        if (ImGui::BeginTable("##pliestable", 3, flags))
        {
            ImGui::TableSetupColumn("##num", ImGuiTableColumnFlags_WidthFixed, 32.0f);
            ImGui::TableSetupColumn("##white", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##black", ImGuiTableColumnFlags_WidthStretch);

            for (std::size_t i = 0; i < records.size(); i += 2)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu.", i / 2 + 1);

                for (int col = 1; col <= 2; ++col)
                {
                    const std::size_t ply = i + col - 1;
                    ImGui::TableSetColumnIndex(col);

                    if (ply >= records.size())
                        continue;

                    const bool isCurrent = (ply + 1 == cursor);
                    const bool isPastCursor = (ply >= cursor);

                    // Black (col 2) is anchored to the right edge: compute the
                    // text width and offset the cursor by the remaining space.
                    if (col == 2)
                    {
                        const float availW = ImGui::GetContentRegionAvail().x;
                        const float textW = ImGui::CalcTextSize(records[ply].san.c_str()).x;
                        if (availW > textW)
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - textW));
                    }

                    if (isCurrent)
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(232, 200, 122, 255));
                    else if (isPastCursor)
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(163, 162, 161, 255));

                    ImGui::PushID(static_cast<int>(ply));

                    const bool clicked =
                        ImGui::Selectable(records[ply].san.c_str(), isCurrent, ImGuiSelectableFlags_AllowOverlap);

                    if (clicked && !gameOver)
                        controller.requestGotoPly(ply + 1);

                    ImGui::PopID();

                    if (isCurrent || isPastCursor)
                        ImGui::PopStyleColor();
                }
            }

            ImGui::EndTable();
        }

        if (cursor > 0 && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::SetWindowFontScale(1.0f);

        ImGui::EndChild();

        const bool canUndo = !gameOver && game.cursor() > 0;
        const bool canRedo = !gameOver && game.cursor() < game.totalMoves();

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