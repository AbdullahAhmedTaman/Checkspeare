file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(REMOVE_ITEM ENGINE_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

add_library(EngineCore
        ${ENGINE_SRC}
)

target_include_directories(EngineCore
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(EngineCore PRIVATE cxx_std_23)

add_executable(Checkspeare
        src/main.cpp
)

target_link_libraries(Checkspeare
        PRIVATE
        EngineCore
)

target_compile_features(Checkspeare PRIVATE cxx_std_23)

# Production configuration.
#
# Works with both single-config generators (Ninja/Makefiles)
# and multi-config generators (Visual Studio/Xcode/Ninja Multi-Config).
set(IS_PRODUCTION_CONFIG
        "$<OR:$<CONFIG:Release>,$<BOOL:${PRODUCTION_BUILD}>>"
)

# Compiler warnings.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(Checkspeare PRIVATE
            /W4
            /wd4100
            /wd4189
    )
else()
    target_compile_options(Checkspeare PRIVATE
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
        target_link_options(Checkspeare PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libgcc>"
        )
    endif()

    if(USE_STATIC_LIBSTDCXX)
        target_link_options(Checkspeare PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libstdc++>"
        )
    elseif(USE_STATIC_LIBCXX)
        target_link_options(Checkspeare PRIVATE
                "$<${IS_PRODUCTION_CONFIG}:-static-libc++>"
        )
    endif()

    # Fully static GNU-style Windows builds.
    if(WIN32)
        target_link_options(Checkspeare PRIVATE
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
    set_property(TARGET Checkspeare PROPERTY
            MSVC_RUNTIME_LIBRARY
            "$<IF:${IS_PRODUCTION_CONFIG},MultiThreaded$<$<CONFIG:Debug>:Debug>,>"
    )
endif()

add_subdirectory(tests)
