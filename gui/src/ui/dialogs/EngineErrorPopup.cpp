#include "EngineErrorPopup.hpp"

#include <imgui.h>

namespace Gui
{

    void renderEngineErrorPopup(std::string& message)
    {
        if (message.empty())
            return;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        ImGui::OpenPopup("Engine Error");

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::BeginPopupModal("Engine Error", nullptr, flags))
        {
            ImGui::TextUnformatted("Could not start the engine.");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::TextDisabled("Check the engine path and try again.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            constexpr float kButtonW = 120.0f;
            const float availW = ImGui::GetContentRegionAvail().x;

            if (availW > kButtonW)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - kButtonW) * 0.5f);

            if (ImGui::Button("OK", ImVec2(kButtonW, 0.0f)))
            {
                message.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Gui