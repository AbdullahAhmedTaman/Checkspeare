#pragma once

#include "BoardGeometry.hpp"
#include "ChessClock.hpp"
#include "Config.hpp"
#include "GuiGameController.hpp"


#include <SFML/Graphics/RenderWindow.hpp>

namespace Gui
{

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, const GuiGameController& controller,
                     const ChessGame& game, const ChessClock& clock);

} // namespace Gui