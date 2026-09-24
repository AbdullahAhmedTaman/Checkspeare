#include "GameOverModal.hpp"

#include "ChessClock.hpp"
#include "GameResult.hpp"

#include <imgui.h>

#include <string>

namespace Gui
{

    namespace
    {
        /// Combined result text: a clock flag-fall takes priority over the
        /// game's own result, because it's what actually ended the game
        /// (the position might still be Ongoing).
        std::string combinedResultText(const GameResult& r, const ChessClock& clock)
        {
            if (clock.flagFallen(WHITE))
                return "White loses on time";
            if (clock.flagFallen(BLACK))
                return "Black loses on time";
            return toString(r);
        }
    } // namespace

    void renderGameOverModal(GuiGameController& controller, const ChessGame& game, const ChessClock& clock)
    {
        const GameResult result = game.result();
        const bool clockOver = clock.flagFallen(WHITE) || clock.flagFallen(BLACK);

        if (result.kind == GameResultKind::Ongoing && !clockOver)
            return;

        if (controller.gameOverModalDismissed())
            return;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        ImGui::OpenPopup("Game Over");

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::BeginPopupModal("Game Over", nullptr, flags))
        {
            const std::string text = combinedResultText(result, clock);

            const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
            const float availW = ImGui::GetContentRegionAvail().x;

            if (availW > textSize.x)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - textSize.x) * 0.5f);

            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.6f, 1.0f), "%s", text.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            constexpr float buttonW = 130.0f;
            constexpr float gap = 20.0f;
            const float groupW = buttonW * 2 + gap;
            const float groupLeft = (availW - groupW) * 0.5f;

            if (groupLeft > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + groupLeft);

            if (ImGui::Button("New Game", ImVec2(buttonW, 0.0f)))
            {
                controller.requestNewGame();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine(0.0f, gap);

            if (ImGui::Button("Close", ImVec2(buttonW, 0.0f)))
            {
                controller.dismissGameOverModal();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Gui