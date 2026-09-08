#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <exception>
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

#include "renderUI.h"

int main()
{
    try
    {
        auto window = initWindow();
        // this is non-copybable object which handlded by c++17 RVO
        Config conf;
        windowRenderUI(window, conf);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal , no Init happened " << e.what() << '\n';
        return -1;
    }
    return 0;
}