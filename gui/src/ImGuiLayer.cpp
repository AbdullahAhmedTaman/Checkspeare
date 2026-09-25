#include "ImGuiLayer.hpp"

#include "ClockPanel.hpp"
#include "GameOverModal.hpp"
#include "StatusPanel.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

namespace Gui
{

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, GuiGameController& controller,
                     const ChessGame& game, const ChessClock& clock, ImFont* clockFont)
    {
        renderStatusPanel(controller, game);
        renderClockPanel(clock, game, clockFont);

        if (conf.showDemoWindow)
            ImGui::ShowDemoWindow(&conf.showDemoWindow);

        // Draw the game-over modal last so it renders on top of every panel.
        renderGameOverModal(controller, game, clock);

        (void) window;
        (void) geo;
    }

} // namespace Gui