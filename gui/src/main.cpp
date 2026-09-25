#include "Application.hpp"
#include "Config.hpp"
#include "assets/TextureLibrary.hpp"
#include "audio/SoundLibrary.hpp"
#include "board/attacks.h"
#include "board/zobrist.h"
#include <iostream>

int main()
{
    try
    {
        init_attacks();
        Zobrist::init();

        Assets::TextureLibrary textures;
        textures.loadAllPieceTextures(RESOURCES_PATH "/textures/pieces/");
        textures.loadAllSquareTextures(RESOURCES_PATH "/textures/squares/");

        Assets::SoundLibrary sounds;
        sounds.loadAll(RESOURCES_PATH "/sounds/");

        Config conf;
        Application app(conf, textures, sounds);
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fatal] " << e.what() << '\n';
        return 1;
    }
    return 0;
}