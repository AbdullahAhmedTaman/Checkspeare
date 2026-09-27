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

target_compile_features(ChessGUI PRIVATE cxx_std_23)

# Production configuration.
#
# Works with both single-config generators (Ninja/Makefiles)
# and multi-config generators (Visual Studio/Xcode/Ninja Multi-Config).
set(IS_PRODUCTION_CONFIG
        "$<OR:$<CONFIG:Release>,$<BOOL:${PRODUCTION_BUILD}>>"
)

# Compiler warnings.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(ChessGUI PRIVATE
            /W4
            /wd4100
            /wd4189
    )
else()
    target_compile_options(ChessGUI PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )
endif()

# Static compiler runtimes for GNU-style compilers.
#
# GCC and Clang using the GNU frontend are handled here.
# Only the compiler runtimes are statically linked on Unix-like systems.
# A fully static link is additionally requested on Windows.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")

    set(USE_STATIC_LIBGCC FALSE)
    set(USE_STATIC_LIBSTDCXX FALSE)
    set(USE_STATIC_LIBCXX FALSE)

    # Detect the C++ standard library used by the compiler.
    list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "stdc++" HAS_LIBSTDCXX)
    list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "c++" HAS_LIBCXX)

    if(HAS_LIBSTDCXX GREATER_EQUAL 0)
        set(USE_STATIC_LIBSTDCXX TRUE)
    elseif(HAS_LIBCXX GREATER_EQUAL 0)
        set(USE_STATIC_LIBCXX TRUE)
    endif()

    # Detect GCC's runtime library.
    list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "gcc" HAS_LIBGCC)
    list(FIND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "gcc_s" HAS_LIBGCC_S)

    if(HAS_LIBGCC GREATER_EQUAL 0 OR HAS_LIBGCC_S GREATER_EQUAL 0)
        set(USE_STATIC_LIBGCC TRUE)
    endif()

    if(USE_STATIC_LIBGCC)
        target_link_options(ChessGUI PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libgcc>"
        )
    endif()

    if(USE_STATIC_LIBSTDCXX)
        target_link_options(ChessGUI PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libstdc++>"
        )
    elseif(USE_STATIC_LIBCXX)
        target_link_options(ChessGUI PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libc++>"
        )
    endif()

    # Fully static GNU-style Windows builds.
    if(WIN32)
        target_link_options(ChessGUI PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static>"
        )
    endif()

endif()

# MSVC / clang-cl runtime.
#
# Production builds use the static MSVC runtime:
#   Release -> /MT
#   Debug   -> /MTd
#
# Non-production configurations retain CMake's default runtime selection.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set_property(TARGET ChessGUI PROPERTY
            MSVC_RUNTIME_LIBRARY
            "$<IF:${IS_PRODUCTION_CONFIG},MultiThreaded$<$<CONFIG:Debug>:Debug>,>"
    )
endif()

# Compile definitions.
target_compile_definitions(ChessGUI PRIVATE
        "$<${IS_PRODUCTION_CONFIG}:PRODUCTION_BUILD=1>"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:PRODUCTION_BUILD=0>"

        "$<${IS_PRODUCTION_CONFIG}:RESOURCES_PATH=\"assets/\">"
        "$<$<NOT:${IS_PRODUCTION_CONFIG}>:RESOURCES_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/\">"
)

# Production application settings.
if(WIN32)
    set_property(TARGET ChessGUI PROPERTY
            WIN32_EXECUTABLE
            "${IS_PRODUCTION_CONFIG}"
    )
elseif(APPLE)
    set_property(TARGET ChessGUI PROPERTY
            MACOSX_BUNDLE
            "${IS_PRODUCTION_CONFIG}"
    )
endif()

# Windows resource file.
if(WIN32)
    target_sources(ChessGUI PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/app.rc"
    )
endif()

# Copy runtime assets beside the executable.
#
# The destination is recreated on every successful build so removed
# source assets cannot remain as stale files in the build directory.
add_custom_command(TARGET ChessGUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "$<TARGET_FILE_DIR:ChessGUI>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:ChessGUI>/assets"
)

target_link_libraries(ChessGUI
        PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        EngineCore
)