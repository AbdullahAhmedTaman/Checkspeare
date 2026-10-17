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

configure_gnu_production_runtime(CheckspeareGUI)

# Compile definitions.
target_compile_definitions(CheckspeareGUI PRIVATE
        "$<${IS_PRODUCTION_CONFIG}:PRODUCTION_BUILD=1>"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:PRODUCTION_BUILD=0>"

        "$<${IS_PRODUCTION_CONFIG}:RESOURCES_PATH=\"assets/\">"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:RESOURCES_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/\">"
)

# Production application settings.
if(WIN32)
        set_property(TARGET CheckspeareGUI PROPERTY
                        WIN32_EXECUTABLE
                        "${IS_PRODUCTION_CONFIG}"
        )
elseif(APPLE)
    set_property(TARGET CheckspeareGUI PROPERTY
            MACOSX_BUNDLE
            "${IS_PRODUCTION_CONFIG}"
    )
endif()

# Windows resource file.
if(WIN32)
    target_sources(CheckspeareGUI PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/app.rc"
    )
endif()

# Copy runtime assets beside the executable.
#
# The destination is recreated on every successful build so removed
# source assets cannot remain as stale files in the build directory.
add_custom_command(TARGET CheckspeareGUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "$<TARGET_FILE_DIR:CheckspeareGUI>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:CheckspeareGUI>/assets"
)

target_link_libraries(CheckspeareGUI
        PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        EngineCore
)