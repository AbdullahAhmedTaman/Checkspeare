#include "Application.hpp"
#include "AssetsManager.h"
#include "Config.h"
#include "attacks.h"
#include "zobrist.h"
#include <iostream>

int main()
{
    try
    {
        init_attacks();
        Zobrist::init();

        Assets::AssetManager assets;
        assets.loadAllPieceTextures("gui/assets/pieces/");
        assets.loadAllSquareTextures("gui/assets/squares/");

        Config conf;
        Application app(conf, assets);
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fatal] " << e.what() << '\n';
        return 1;
    }
    return 0;
}