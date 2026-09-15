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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/include
)

target_link_libraries(ChessGUI
    PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        ChessEngine
)
