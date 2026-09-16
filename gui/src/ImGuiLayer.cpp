#include "ImGuiLayer.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
        /// The single place where an ActionResult is turned into human text.
        /// Sound effects, a move-log panel, or telemetry would hook in
        /// alongside this function without touching GuiGameController.
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
            }
            return {};
        }
    } // namespace

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, const GuiGameController& controller)
    {
        ImGui::Begin("Chess Engine Test");

        ImGui::Text("SFML + ImGui are working!");
        ImGui::Separator();

        const auto size = window.getSize();
        ImGui::Text("Window size: %u x %u", size.x, size.y);

        ImGui::SliderFloat("Square Size", &geo.squareSize, 20.0f, 150.0f);

        // SliderFloat2 needs a contiguous float[2]; copy in/out rather than
        // aliasing sf::Vector2f's storage.
        float origin[2] = {geo.boardOrigin.x, geo.boardOrigin.y};
        if (ImGui::SliderFloat2("Board Position", origin, 0.0f, 300.0f))
            geo.boardOrigin = {origin[0], origin[1]};

        ImGui::Checkbox("Flip Board", &geo.flipped);
        ImGui::Separator();
        ImGui::Text("Status:");
        const std::string status = formatActionResult(controller.result());
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", status.c_str());

        ImGui::Separator();
        ImGui::Checkbox("Show ImGui Demo", &conf.showDemoWindow);

        ImGui::End();

        if (conf.showDemoWindow)
            ImGui::ShowDemoWindow(&conf.showDemoWindow);
    }

} // namespace Gui