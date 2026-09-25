#include "GameSetupPanel.hpp"

#include <imgui.h>

#include <cfloat>
#include <cstdint>
#include <cstring>
#include <string>

namespace Gui
{

    namespace
    {
        constexpr const char* kPopupId = "New Game";

        struct Preset
        {
            const char* label;
            std::int64_t initialMs;
            std::int64_t incrementMs;
        };

        // initialMs < 0 marks "custom": selecting it leaves the current
        // values untouched so the user can edit them in the fields below.
        constexpr Preset kPresets[] = {
            {"Bullet 1+0", 60 * 1000, 0},           {"Blitz 3+2", 3 * 60 * 1000, 2 * 1000},
            {"Blitz 5+3", 5 * 60 * 1000, 3 * 1000}, {"Rapid 10+5", 10 * 60 * 1000, 5 * 1000},
            {"Classical 30+0", 30 * 60 * 1000, 0},  {"Custom", -1, 0},
        };
        constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

        /// Mutable char buffers for the path fields. ImGui::InputText needs
        /// a raw char*, and the pending struct holds std::string, so the
        /// two are kept in sync at the boundary. Statics because there is
        /// one modal per process.
        struct PathBuffers
        {
            char white[512] = {};
            char black[512] = {};
            bool initialized = false;
        };

        /// Finds the preset whose (initial, increment) matches the given
        /// time control, or kPresetCount - 1 (Custom) if none does. Used
        /// to seed the combo box on the first frame so the displayed preset
        /// matches the pending values.
        int presetFor(const ChessClock::TimeControl& tc)
        {
            for (int i = 0; i < kPresetCount - 1; ++i)
            {
                if (kPresets[i].initialMs == tc.initialMs && kPresets[i].incrementMs == tc.incrementMs)
                    return i;
            }
            return kPresetCount - 1;
        }
    } // namespace

    std::optional<GameSetup> renderGameSetupModal(GameSetup& pending, bool shouldOpen)
    {
        static PathBuffers paths;
        static int selectedPreset = -1;

        if (!paths.initialized)
        {
            paths.initialized = true;
            std::strncpy(paths.white, pending.whiteEnginePath.c_str(), sizeof(paths.white) - 1);
            std::strncpy(paths.black, pending.blackEnginePath.c_str(), sizeof(paths.black) - 1);
            selectedPreset = presetFor(pending.timeControl);
        }

        // Host window: the modal has to be opened from within some window's
        // ID stack. This invisible host provides one without adding a
        // visible panel to the layout.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));
        ImGui::Begin("##GameSetupHost", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);

        if (shouldOpen)
            ImGui::OpenPopup(kPopupId);

        std::optional<GameSetup> committed;

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            // Scoped field styling. The global theme disables frame borders
            // and sets FrameBg close to WindowBg, which makes InputText and
            // Combo read as flat text on the panel. These pushes darken the
            // field fill, enable a 1px border, and add internal padding so
            // each editable widget reads as a slot. Everything else -- the
            // section separators, the button, the modal chrome -- is
            // untouched.
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.165f, 0.163f, 0.161f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.180f, 0.178f, 0.176f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.195f, 0.193f, 0.191f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.30f, 0.36f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

            // Time control
            ImGui::SeparatorText("Time control");

            if (ImGui::BeginCombo("Preset", kPresets[selectedPreset].label))
            {
                for (int i = 0; i < kPresetCount; ++i)
                {
                    if (ImGui::Selectable(kPresets[i].label, i == selectedPreset))
                    {
                        selectedPreset = i;
                        if (kPresets[i].initialMs >= 0)
                        {
                            pending.timeControl.initialMs = kPresets[i].initialMs;
                            pending.timeControl.incrementMs = kPresets[i].incrementMs;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            int initialMinutes = static_cast<int>(pending.timeControl.initialMs / 60000);
            int incrementSeconds = static_cast<int>(pending.timeControl.incrementMs / 1000);

            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt("Minutes", &initialMinutes))
            {
                // Minimum one minute. Zero would make the flag fall on the
                // first clock tick, before either side can move.
                if (initialMinutes < 1)
                    initialMinutes = 1;
                pending.timeControl.initialMs = static_cast<std::int64_t>(initialMinutes) * 60000;
                selectedPreset = presetFor(pending.timeControl);
            }

            ImGui::SameLine();

            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt("Increment (s)", &incrementSeconds))
            {
                if (incrementSeconds < 0)
                    incrementSeconds = 0;
                pending.timeControl.incrementMs = static_cast<std::int64_t>(incrementSeconds) * 1000;
                selectedPreset = presetFor(pending.timeControl);
            }

            // Players
            ImGui::SeparatorText("Players");

            const char* kinds[] = {"Human", "Engine"};
            int whiteKind = (pending.whiteKind == PlayerKind::Engine) ? 1 : 0;
            int blackKind = (pending.blackKind == PlayerKind::Engine) ? 1 : 0;

            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::Combo("White", &whiteKind, kinds, 2))
                pending.whiteKind = (whiteKind == 1) ? PlayerKind::Engine : PlayerKind::Human;

            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::Combo("Black", &blackKind, kinds, 2))
                pending.blackKind = (blackKind == 1) ? PlayerKind::Engine : PlayerKind::Human;

            // Engine paths
            if (pending.whiteKind == PlayerKind::Engine || pending.blackKind == PlayerKind::Engine)
            {
                ImGui::SeparatorText("Engine binaries");

                if (pending.whiteKind == PlayerKind::Engine)
                {
                    ImGui::TextDisabled("White engine");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputText("##whitePath", paths.white, sizeof(paths.white)))
                        pending.whiteEnginePath = paths.white;
                }

                if (pending.blackKind == PlayerKind::Engine)
                {
                    ImGui::TextDisabled("Black engine");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputText("##blackPath", paths.black, sizeof(paths.black)))
                        pending.blackEnginePath = paths.black;
                }
            }

            // Actions
            ImGui::Separator();

            if (ImGui::Button("Start game", ImVec2(140.0f, 0.0f)))
            {
                committed = pending;
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            ImGui::EndPopup();
        }

        ImGui::End();

        return committed;
    }

} // namespace Gui