#pragma once

#include "BoardGeometry.hpp"
#include "Config.h"
#include "GuiGameController.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace Gui
{

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, const GuiGameController& controller);

} // namespace Gui