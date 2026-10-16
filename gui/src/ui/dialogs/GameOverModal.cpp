#include "GameOverModal.hpp"

#include "game/ChessClock.hpp"
#include "game/GameResult.hpp"

#include <imgui.h>

#include <string>

namespace Gui
{

    namespace
    {
        std::string combinedResultText(const GameResult& r, const ChessClock& clock)
        {
            if (clock.flagFallen(WHITE))
                return "White loses on time";
            if (clock.flagFallen(BLACK))
                return "Black loses on time";
            return toString(r);
        }
    } // namespace

    void renderGameOverModal(GuiGameController& controller, const ChessGame& game, const ChessClock& clock, bool setupModalOpen)
    {
        // While the setup modal is on screen, the game-over modal must stay
        // hidden. The game state still carries the previous game's result
        // (checkmate, or a fallen flag) until the new setup is committed
        // and applyGameSetup resets it, so without this guard the checks
        // below would re-open the modal and produce two simultaneous
        // popups fighting for the screen.
        if (setupModalOpen)
            return;

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

            if (ImGui::Button("Quit", ImVec2(buttonW, 0.0f)))
            {
                controller.requestQuit();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Gui