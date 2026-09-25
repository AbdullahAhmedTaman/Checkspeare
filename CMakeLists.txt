cmake_minimum_required(VERSION 3.28)
project(Chess VERSION 1.0 LANGUAGES CXX)
option(PRODUCTION_BUILD "Make this a production build!" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/bin
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

set(IMGUI_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-docking/imgui"
        CACHE PATH
        "Path to the bundled ImGui source"
        FORCE
)

set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL
        "Use the bundled SFML target instead of searching for an installed package"
        FORCE
)

add_subdirectory(external/SFML-3.1.0)
add_subdirectory(external/imgui-docking)
add_subdirectory(external/imgui-sfml)
add_subdirectory(external/Catch2-3.8.1)


enable_testing()

add_subdirectory(engine)
add_subdirectory(gui)
add_subdirectory(gui/tests)