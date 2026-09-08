#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <exception>
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

#include "renderUI/AssetManager.h"
#include "renderUI/renderUI.h"


int main()
{
    try
    {
        renderUI::AssetManager assetManager;

        if (!assetManager.loadAllPieceTextures("gui/assets/pieces/"))
        {
            std::cerr << "Failed to load piece textures. Check directory paths!\n";
            return -1;
        }

        auto window = renderUI::initWindow();
        Config conf;
        renderUI::windowRenderUI(window, conf, assetManager);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return -1;
    }
    return 0;
}