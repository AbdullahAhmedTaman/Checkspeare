// StatusPanel.cpp
#include "StatusPanel.hpp"

#include "../game/GameResult.hpp"

#include <imgui.h>

#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
        std::string formatActionResult(const ActionResult& result)
        {
            std::ostringstream oss;
            switch (result.kind)
            {
                case ActionResult::Kind::Ready:
                    return "Ready";
                case ActionResult::Kind::NotYourTurn:
                    return "Not your turn";
                case ActionResult::Kind::Dragging:
                    oss << "Dragging " << result.piece << " from " << result.from;
                    return oss.str();
                case ActionResult::Kind::MoveCancelled:
                    return "Move cancelled";
                case ActionResult::Kind::DroppedOutsideBoard:
                    return "Dropped outside board";
                case ActionResult::Kind::MovePlayed:
                    oss << "Played " << result.from << "-" << result.to;
                    return oss.str();
                case ActionResult::Kind::IllegalMove:
                    oss << "Illegal move " << result.from << "-" << result.to;
                    return oss.str();
                case ActionResult::Kind::PromotionPending:
                    return "Choose promotion piece on board";
            }
            return {};
        }
    } // namespace

    void renderStatusPanel(GuiGameController& controller, const ChessGame& game, bool canResign)
    {
        ImGui::Begin("Game");

        const bool whiteToMove = (game.sideToMove() == WHITE);

        ImGui::TextDisabled("TURN");
        ImGui::SameLine();

        const ImU32 dotColor = whiteToMove ? IM_COL32(242, 241, 240, 255) : IM_COL32(30, 29, 28, 255);
        const ImU32 ringColor = whiteToMove ? IM_COL32(184, 183, 182, 255) : IM_COL32(128, 127, 126, 255);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            const float lineH = ImGui::GetTextLineHeight();
            const ImVec2 center = ImVec2(cur.x + 5.0f, cur.y + lineH * 0.5f);
            dl->AddCircleFilled(center, 5.0f, dotColor);
            dl->AddCircle(center, 5.0f, ringColor, 0, 1.2f);
            ImGui::Dummy(ImVec2(16.0f, lineH));
        }

        ImGui::SameLine();
        ImGui::Text("%s", whiteToMove ? "White to move" : "Black to move");

        ImGui::Spacing();

        ImGui::TextDisabled("STATUS");
        ImGui::SameLine();
        const std::string status = formatActionResult(controller.result());
        ImGui::TextColored(ImVec4(0.94f, 0.72f, 0.30f, 1.0f), "%s", status.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        constexpr float kResignButtonWidth = 120.0f;
        const float availW = ImGui::GetContentRegionAvail().x;

        if (availW > kResignButtonWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - kResignButtonWidth) * 0.5f);

        ImGui::BeginDisabled(!canResign);
        if (ImGui::Button("Resign", ImVec2(kResignButtonWidth, 0.0f)))
            controller.requestResign(game.sideToMove());
        ImGui::EndDisabled();

        ImGui::Spacing();

        const GameResult result = game.result();
        const std::string resultText = toString(result);
        if (!resultText.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const ImVec2 textSize = ImGui::CalcTextSize(resultText.c_str());
            const float cardH = textSize.y + 22.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.34f, 0.14f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.86f, 0.42f, 0.34f, 1.0f));
            ImGui::BeginChild("##result", ImVec2(0, cardH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float offsetX = (avail.x - textSize.x) * 0.5f;
            const float offsetY = (avail.y - textSize.y) * 0.5f;

            if (offsetX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            if (offsetY > 0.0f)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

            ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.58f, 1.0f), "%s", resultText.c_str());

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        ImGui::End();
    }

} // namespace Gui