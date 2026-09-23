#include "Application.hpp"
#include "Config.hpp"
#include "TextureLibrary.hpp"
#include "attacks.h"
#include "zobrist.h"
#include <iostream>

int main()
{
    try
    {
        init_attacks();
        Zobrist::init();

        Assets::TextureLibrary textures;
        textures.loadAllPieceTextures("gui/assets/textures/pieces/");
        textures.loadAllSquareTextures("gui/assets/textures/squares/");

        Config conf;
        Application app(conf, textures);
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fatal] " << e.what() << '\n';
        return 1;
    }
    return 0;
}