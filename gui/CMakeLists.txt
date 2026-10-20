add_executable(CheckspeareGUI)

file(GLOB_RECURSE GUI_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(CheckspeareGUI
        PRIVATE
        ${GUI_SRC}
)

target_include_directories(CheckspeareGUI
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(CheckspeareGUI PRIVATE cxx_std_23)

# Compiler warnings.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(CheckspeareGUI PRIVATE
            /W4
            /wd4100
            /wd4189
    )
else()
    target_compile_options(CheckspeareGUI PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )
endif()

if(WIN32)
    set_property(TARGET CheckspeareGUI PROPERTY WIN32_EXECUTABLE "${IS_PRODUCTION_CONFIG}")
elseif(APPLE)
    set_property(TARGET CheckspeareGUI PROPERTY MACOSX_BUNDLE ON)
endif()

# Production reads assets relative to the executable at runtime, so only
# non-production builds get a compile-time path.
target_compile_definitions(CheckspeareGUI PRIVATE
        "$<${IS_PRODUCTION_CONFIG}:PRODUCTION_BUILD=1>"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:PRODUCTION_BUILD=0>"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:RESOURCES_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/\">"
)

if(APPLE)
    set(ASSET_DEST "$<TARGET_BUNDLE_CONTENT_DIR:CheckspeareGUI>/Resources/assets")
else()
    set(ASSET_DEST "$<TARGET_FILE_DIR:CheckspeareGUI>/assets")
endif()

add_custom_command(TARGET CheckspeareGUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${ASSET_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${ASSET_DEST}"
)

# Windows resource file.
if(WIN32)
    target_sources(CheckspeareGUI PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/app.rc"
    )
endif()

target_link_libraries(CheckspeareGUI
        PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        EngineCore
)

if(APPLE)
    # The GUI is a .app bundle and the engine is a plain executable,
    # this puts a copy of the engine next to the GUI binary inside the bundle.
    add_dependencies(CheckspeareGUI CheckspeareEngine)
    add_custom_command(TARGET CheckspeareGUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:CheckspeareEngine>"
                    "$<TARGET_FILE_DIR:CheckspeareGUI>"
    )
endif()
configure_portable_runtime(CheckspeareGUI)