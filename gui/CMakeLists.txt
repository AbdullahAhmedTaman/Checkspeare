add_executable(ChessGUI)

file(GLOB_RECURSE GUI_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(ChessGUI
        PRIVATE
        ${GUI_SRC}
)

target_include_directories(ChessGUI
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ChessGUI PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR PRODUCTION_BUILD)
        target_link_options(ChessGUI PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
        )
        
    endif()

elseif(MSVC)
    target_compile_options(ChessGUI PRIVATE
            /W4
            /wd4100
            /wd4189
    )

    if(PRODUCTION_BUILD)
        set_property(TARGET ChessGUI PROPERTY
                MSVC_RUNTIME_LIBRARY
                "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR PRODUCTION_BUILD)
    target_compile_definitions(ChessGUI PRIVATE
            RESOURCES_PATH="assets/"
            PRODUCTION_BUILD=1
    )
else()
    target_compile_definitions(ChessGUI PRIVATE
            RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
            PRODUCTION_BUILD=0
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR PRODUCTION_BUILD)

    if(WIN32)
        set_target_properties(ChessGUI PROPERTIES
                WIN32_EXECUTABLE TRUE
        )
    elseif(APPLE)
        set_target_properties(ChessGUI PROPERTIES
                MACOSX_BUNDLE TRUE
        )
    endif()

endif()

if(WIN32)
    target_sources(ChessGUI PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/app.rc"
    )
endif()

target_link_libraries(ChessGUI
        PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        EngineCore
)