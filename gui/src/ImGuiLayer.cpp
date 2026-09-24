#include "ImGuiLayer.hpp"

#include "ClockPanel.hpp"
#include "StatusPanel.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

namespace Gui
{

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, const GuiGameController& controller,
                     const ChessGame& game, const ChessClock& clock, ImFont* clockFont)
    {
        renderStatusPanel(controller, game);
        renderClockPanel(clock, game, clockFont);

        if (conf.showDemoWindow)
            ImGui::ShowDemoWindow(&conf.showDemoWindow);

        // `window` and `geo` are unused here now that the status panel no
        // longer draws the flip/demo toggles or the window-size footer.
        // Kept in the signature so Application doesn't need to change.
        (void) window;
        (void) geo;
    }

} // namespace Gui