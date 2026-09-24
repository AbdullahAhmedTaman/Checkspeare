#include "Dockspace.hpp"

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder*, ImGuiDockNode

namespace Gui
{

    void beginMainDockspace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                           ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Build a default layout once per session, and only when the
        // dockspace is still a single leaf node (i.e. nothing was restored
        // from the .ini).
        static bool layoutInitialized = false;
        if (!layoutInitialized)
        {
            layoutInitialized = true;

            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
            if (node != nullptr && node->IsLeafNode())
            {
                ImGuiID center = dockspaceId;
                ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
                ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.22f, nullptr, &center);
                ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f, nullptr, &right);

                ImGui::DockBuilderDockWindow("Board", center);
                ImGui::DockBuilderDockWindow("Clock", bottom);
                ImGui::DockBuilderDockWindow("Game", bottom);
                ImGui::DockBuilderDockWindow("Engine", right);
                ImGui::DockBuilderDockWindow("Moves", rightBottom);
                ImGui::DockBuilderFinish(dockspaceId);
            }
        }

        ImGui::End();
    }

} // namespace Gui