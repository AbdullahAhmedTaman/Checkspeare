#include "EngineSettingsPanel.hpp"

#include <imgui.h>

#include <cstring>
#include <exception>
#include <string>

namespace Gui
{

    namespace
    {
        /// Per-side UI state. Statics inside renderEngineSlot would be
        /// shared across both tabs; making this a struct lets each tab
        /// keep its own edit buffer and error string.
        struct SlotState
        {
            char pathBuffer[512] = {};
            std::string lastError;
            bool bufferInitialized = false;
        };

        void renderSlot(SlotState& state, UciEngine& engine, EnginePlayer& player, std::string& path)
        {
            if (!state.bufferInitialized)
            {
                state.bufferInitialized = true;
                std::strncpy(state.pathBuffer, path.c_str(), sizeof(state.pathBuffer) - 1);
                state.pathBuffer[sizeof(state.pathBuffer) - 1] = '\0';
            }

            // Status
            ImGui::TextDisabled("STATUS");
            ImGui::SameLine();

            const bool running = engine.isRunning();
            const bool ready = engine.isReady();
            const bool thinking = player.thinking();

            if (!running)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Not running");
            else if (thinking)
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Searching...");
            else if (ready)
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Ready");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Starting...");

            ImGui::Separator();

            // Path input
            ImGui::TextDisabled("ENGINE PATH");
            ImGui::SetNextItemWidth(-FLT_MIN);

            if (ImGui::InputText("##path", state.pathBuffer, sizeof(state.pathBuffer)))
                path = state.pathBuffer;

            // Lifecycle
            if (ImGui::Button("Start"))
            {
                state.lastError.clear();
                try
                {
                    engine.start(path);
                }
                catch (const std::exception& e)
                {
                    state.lastError = e.what();
                }
            }

            ImGui::SameLine();

            ImGui::BeginDisabled(!running);
            if (ImGui::Button("Stop"))
            {
                engine.stop();
                state.lastError.clear();
            }
            ImGui::EndDisabled();

            if (!state.lastError.empty())
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("%s", state.lastError.c_str());
                ImGui::PopStyleColor();
            }
        }
    } // namespace

    void renderEngineSettingsPanel(Config& conf, EnginePlayer& whitePlayer, UciEngine& whiteEngine, EnginePlayer& blackPlayer,
                                   UciEngine& blackEngine)
    {
        ImGui::Begin("Engine Settings");

        static SlotState whiteState;
        static SlotState blackState;

        if (ImGui::BeginTabBar("##engines"))
        {
            if (ImGui::BeginTabItem("White"))
            {
                renderSlot(whiteState, whiteEngine, whitePlayer, conf.whiteEnginePath);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Black"))
            {
                renderSlot(blackState, blackEngine, blackPlayer, conf.blackEnginePath);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

} // namespace Gui