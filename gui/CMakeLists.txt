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
)

target_link_libraries(ChessGUI
    PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
)

if(WIN32)

    add_custom_command(
        TARGET ChessGUI
        POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SFML::System>
            $<TARGET_FILE_DIR:ChessGUI>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SFML::Window>
            $<TARGET_FILE_DIR:ChessGUI>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SFML::Graphics>
            $<TARGET_FILE_DIR:ChessGUI>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SFML::Audio>
            $<TARGET_FILE_DIR:ChessGUI>
    )

endif()