#include "StatusPanel.hpp"

#include "GameResult.hpp"

#include <imgui.h>

#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
        /// The single place where ActionResult is turned into human text.
        /// Additional consumers (sound, telemetry, logging) hook in here
        /// without touching GuiGameController.
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

        /// Human-readable text for the game-over banner, or empty string
        /// when the game is still ongoing.
        std::string formatGameResult(const GameResult& r)
        {
            switch (r.kind)
            {
                case GameResultKind::Ongoing:
                    return "";
                case GameResultKind::Checkmate:
                    return (r.winner == WHITE) ? "Checkmate - White wins" : "Checkmate - Black wins";
                case GameResultKind::Stalemate:
                    return "Stalemate - Draw";
                case GameResultKind::FiftyMove:
                    return "Draw by fifty-move rule";
                case GameResultKind::Threefold:
                    return "Draw by threefold repetition";
                case GameResultKind::InsufficientMaterial:
                    return "Draw by insufficient material";
            }
            return "";
        }
    } // namespace

    void renderStatusPanel(GuiGameController& controller, const ChessGame& game)
    {
        ImGui::Begin("Game");

        // Turn indicator
        const bool whiteToMove = (game.sideToMove() == WHITE);

        ImGui::TextDisabled("TURN");
        ImGui::SameLine();

        // A small colored dot next to the label: white for White, dark for
        // Black. Uses a border so Black is visible against the panel bg.
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

        // Controller feedback
        ImGui::TextDisabled("STATUS");
        ImGui::SameLine();
        const std::string status = formatActionResult(controller.result());
        ImGui::TextColored(ImVec4(0.91f, 0.78f, 0.48f, 1.00f), "%s", status.c_str());

        // Result banner (only when game is over)
        const GameResult result = game.result();
        const std::string resultText = formatGameResult(result);
        if (!resultText.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const ImVec2 textSize = ImGui::CalcTextSize(resultText.c_str());

            // The card holds a centered title line and a centered New Game
            // button below it.
            const float buttonH = ImGui::GetFrameHeight();
            const float cardH = textSize.y + buttonH + 34.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.24f, 0.12f, 0.10f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.78f, 0.44f, 0.34f, 1.00f));
            ImGui::BeginChild("##result", ImVec2(0, cardH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

            const float availW = ImGui::GetContentRegionAvail().x;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - textSize.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
            ImGui::TextColored(ImVec4(0.98f, 0.90f, 0.72f, 1.00f), "%s", resultText.c_str());

            // New Game button, centered under the title.
            constexpr float buttonW = 140.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - buttonW) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

            if (ImGui::Button("New Game", ImVec2(buttonW, 0.0f)))
                controller.requestNewGame();

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        ImGui::End();
    }

} // namespace Gui