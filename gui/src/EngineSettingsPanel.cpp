#include "EngineSettingsPanel.hpp"

#include <imgui.h>

#include <cstring>
#include <exception>

namespace Gui
{

    void renderEngineSettingsPanel(Config& conf, EnginePlayer& enginePlayer, UciEngine& engine)
    {
        ImGui::Begin("Engine Settings");

        // Edit buffer and error string are statics: one panel instance
        // exists per process, and both are UI-only scratch state. Keeping
        // them here rather than on Application avoids threading a buffer
        // through the class for a value nothing else reads.
        static char pathBuffer[512] = {};
        static std::string lastError;
        static bool bufferInitialized = false;

        if (!bufferInitialized)
        {
            bufferInitialized = true;
            std::strncpy(pathBuffer, conf.enginePath.c_str(), sizeof(pathBuffer) - 1);
            pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        }

        // Status
        ImGui::TextDisabled("STATUS");
        ImGui::SameLine();

        const bool running = engine.isRunning();
        const bool ready = engine.isReady();
        const bool thinking = enginePlayer.thinking();

        if (!running)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Not running");
        else if (thinking)
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Thinking...");
        else if (ready)
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Ready");
        else
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Starting...");

        ImGui::Separator();

        // Path input
        ImGui::TextDisabled("ENGINE PATH");

        // -FLT_MIN means "take the rest of the line" without reserving
        // room for anything to the right. Requires <cfloat> for FLT_MIN.
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer)))
            conf.enginePath = pathBuffer;

        // Lifecycle
        //
        // Start is always enabled: UciEngine::start() tears down any live
        // process first, so clicking it with the engine already running
        // reloads the binary under the current path. Stop is only useful
        // when something is running.
        if (ImGui::Button("Start"))
        {
            lastError.clear();
            try
            {
                engine.start(conf.enginePath);
            }
            catch (const std::exception& e)
            {
                lastError = e.what();
            }
        }

        ImGui::SameLine();

        ImGui::BeginDisabled(!running);
        if (ImGui::Button("Stop"))
        {
            engine.stop();
            lastError.clear();
        }
        ImGui::EndDisabled();

        if (!lastError.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", lastError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        // Side control
        //
        // Independent of whether the engine is loaded: the toggles set a
        // preference, and EnginePlayer picks it up the moment the engine
        // becomes ready and it is that side's turn.
        ImGui::TextDisabled("PLAY AS ENGINE");

        bool whiteEngine = enginePlayer.isSideControlled(WHITE);
        if (ImGui::Checkbox("White##engine", &whiteEngine))
            enginePlayer.setSideControlled(WHITE, whiteEngine);

        bool blackEngine = enginePlayer.isSideControlled(BLACK);
        if (ImGui::Checkbox("Black##engine", &blackEngine))
            enginePlayer.setSideControlled(BLACK, blackEngine);

        ImGui::End();
    }

} // namespace Gui