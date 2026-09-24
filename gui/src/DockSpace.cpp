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

        constexpr ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_NoDockingSplit | ImGuiDockNodeFlags_NoDockingOverMe |
                                                 ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoUndocking |
                                                 ImGuiDockNodeFlags_AutoHideTabBar;

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

        static bool layoutInitialized = false;
        if (!layoutInitialized)
        {
            layoutInitialized = true;

            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
            if (node != nullptr && node->IsLeafNode())
            {
                ImGuiID root = dockspaceId;

                ImGuiID rightColumn = ImGui::DockBuilderSplitNode(root, ImGuiDir_Right, 0.18f, nullptr, &root);
                ImGuiID movesCell = ImGui::DockBuilderSplitNode(rightColumn, ImGuiDir_Down, 0.50f, nullptr, &rightColumn);

                ImGuiID bottomStrip = ImGui::DockBuilderSplitNode(root, ImGuiDir_Down, 0.18f, nullptr, &root);

                ImGuiID engineCol = ImGui::DockBuilderSplitNode(root, ImGuiDir_Left, 0.13f, nullptr, &root);

                ImGuiID emptyCorner = ImGui::DockBuilderSplitNode(bottomStrip, ImGuiDir_Right, 0.22f, nullptr, &bottomStrip);

                ImGui::DockBuilderDockWindow("Engine", engineCol);
                ImGui::DockBuilderDockWindow("Board", root);
                ImGui::DockBuilderDockWindow("Clock", bottomStrip);
                ImGui::DockBuilderDockWindow("Game", rightColumn);
                ImGui::DockBuilderDockWindow("Moves", movesCell);

                ImGui::DockBuilderFinish(dockspaceId);
            }
        }

        ImGui::End();
    }

} // namespace Gui