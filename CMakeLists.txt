cmake_minimum_required(VERSION 3.28)
project(Chess VERSION 1.0 LANGUAGES CXX)
option(PRODUCTION_BUILD "Make this a production build!" OFF)

set(IS_PRODUCTION_CONFIG
    "$<OR:$<CONFIG:Release>,$<BOOL:${PRODUCTION_BUILD}>>"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep fetched libraries and project targets on the same MSVC runtime.
# The setting must be established before FetchContent creates dependency targets.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "$<IF:$<OR:$<CONFIG:Release>,$<BOOL:${PRODUCTION_BUILD}>>,MultiThreaded$<$<CONFIG:Debug>:Debug>,MultiThreaded$<$<CONFIG:Debug>:Debug>DLL>"
    )
endif()

function(configure_gnu_production_runtime target)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "stdc++" HAS_LIBSTDCXX)
        list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "c++" HAS_LIBCXX)
        list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "gcc" HAS_LIBGCC)
        list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "gcc_s" HAS_LIBGCC_S)

        if(HAS_LIBGCC GREATER_EQUAL 0 OR HAS_LIBGCC_S GREATER_EQUAL 0)
            target_link_options(${target} PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libgcc>"
            )
        endif()

        if(HAS_LIBSTDCXX GREATER_EQUAL 0)
            target_link_options(${target} PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libstdc++>"
            )
        elseif(HAS_LIBCXX GREATER_EQUAL 0)
            target_link_options(${target} PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libc++>"
            )
        endif()

        if(WIN32)
            target_link_options(${target} PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static>"
            )
        endif()
    endif()
endfunction()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/bin
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
include(FetchContent)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9-docking
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(imgui)
FetchContent_GetProperties(imgui)

set(IMGUI_DIR
    "${imgui_SOURCE_DIR}"
    CACHE PATH "Dear ImGui source directory"
    FORCE
)


set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(imgui-sfml)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)

FetchContent_MakeAvailable(Catch2)


enable_testing()

add_subdirectory(engine)
add_subdirectory(gui)
add_subdirectory(gui/tests)