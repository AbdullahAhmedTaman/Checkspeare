#include "Application.hpp"
#include "Config.hpp"
#include "assets/SoundLibrary.hpp"
#include "assets/TextureLibrary.hpp"
#include "board/attacks.h"
#include "board/zobrist.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int runApplication()
{
    try
    {
        init_attacks();
        Zobrist::init();

        const std::filesystem::path& res = resourcesDir();

        Assets::TextureLibrary textures;
        textures.loadAllPieceTextures(res / "textures" / "pieces");
        textures.loadAllSquareTextures(res / "textures" / "squares");

        Assets::SoundLibrary sounds;
        sounds.loadAll(res / "sounds");

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

int main()
{
    return runApplication();
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return runApplication();
}
#endif