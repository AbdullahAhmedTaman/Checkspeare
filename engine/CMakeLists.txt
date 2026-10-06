option(USE_PEXT "PEXT sliding attacks" ON)
option(ROOK_COMPRESSED "16-bit compressed rook table (needs USE_PEXT)" OFF)

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


# Same instruction-set baseline for HQ and PEXT builds (GCC 11+, use -march=haswell on older GCC).
target_compile_options(EngineCore PUBLIC
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-march=x86-64-v3>)

if(USE_PEXT)
    target_compile_definitions(EngineCore PUBLIC USE_PEXT)
    if(ROOK_COMPRESSED)
        target_compile_definitions(EngineCore PUBLIC ATTACKS_ROOK_COMPRESSED)
    endif()
endif()

message(STATUS "PEXT sliding attacks: ${USE_PEXT}")
message(STATUS "Compressed rook table: ${ROOK_COMPRESSED}")


include(CheckIPOSupported)

check_ipo_supported(
    RESULT ipo_ok
    OUTPUT ipo_msg
    LANGUAGES CXX
)

if(ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
    message(STATUS "IPO/LTO: ENABLED")
else()
    message(STATUS "IPO/LTO: NOT SUPPORTED")
    message(STATUS "IPO/LTO reason: ${ipo_msg}")
endif()

configure_gnu_production_runtime(Checkspeare)

add_subdirectory(tests)
