#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <exception>
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

#include "AssetManager.h"
#include "renderUI.h"

int main()
{
    try
    {
        Assets::AssetManager assetManager;

        assetManager.loadAllPieceTextures("gui/assets/pieces/");
        assetManager.loadAllSquareTextures("gui/assets/squares/");

        auto window = RenderUI::initWindow();
        Config conf;
        RenderUI::windowRenderUI(window, conf, assetManager);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fatal error] Faild to Init due to : " << e.what() << '\n';
        return -1;
    }
    return 0;
}