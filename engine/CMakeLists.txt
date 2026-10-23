include(CheckCXXSourceCompiles)

option(USE_PEXT "PEXT sliding attacks (x86-64 with BMI2 only)" ON)
option(ROOK_COMPRESSED "16-bit compressed rook table (needs USE_PEXT)" OFF)

set(ENGINE_X86_ARCH "x86-64-v3" CACHE STRING
    "-march value for PEXT builds on GCC/Clang: x86-64-v3, native, or empty for -mbmi2 only")

# Target architecture detection.
if(DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE)
    set(_engine_had_try_compile_type TRUE)
    set(_engine_saved_try_compile_type
        "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
else()
    set(_engine_had_try_compile_type FALSE)
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

check_cxx_source_compiles("
#if !(defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)) || defined(_M_ARM64EC)
#error not an x86-64 target
#endif
int main() { return 0; }
" ENGINE_COMPILER_TARGET_X86_64)

if(_engine_had_try_compile_type)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE
        "${_engine_saved_try_compile_type}")
else()
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
endif()

set(ENGINE_TARGET_X86_64 "${ENGINE_COMPILER_TARGET_X86_64}")

if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
    set(ENGINE_TARGET_X86_64 FALSE)
endif()

message(STATUS "Engine target is x86-64: ${ENGINE_TARGET_X86_64}")

# Decide whether PEXT can be enabled.
set(ENGINE_USE_PEXT "${USE_PEXT}")

if(USE_PEXT AND NOT ENGINE_TARGET_X86_64)
    set(ENGINE_USE_PEXT OFF)
    message(WARNING
        "USE_PEXT=ON requested, but the target is not (single-arch) x86-64. "
        "Disabling PEXT and using the non-PEXT implementation."
    )
endif()

if(ROOK_COMPRESSED AND NOT ENGINE_USE_PEXT)
    message(WARNING
        "ROOK_COMPRESSED requires PEXT. "
        "The compressed rook table option will be ignored."
    )
endif()

# Core library.
file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(REMOVE_ITEM ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

option(
    ENGINE_ENABLE_LTO
    "Enable IPO/LTO for the standalone engine when supported"
    ON
)

include(CheckIPOSupported)

if(ENGINE_ENABLE_LTO)
    check_ipo_supported(
        RESULT ENGINE_IPO_SUPPORTED
        OUTPUT ENGINE_IPO_ERROR
        LANGUAGES CXX
    )
else()
    set(ENGINE_IPO_SUPPORTED FALSE)
endif()

if(ENGINE_ENABLE_LTO AND ENGINE_IPO_SUPPORTED)
    message(STATUS "IPO/LTO Engine: ENABLED")
else()
    message(STATUS "IPO/LTO Engine: DISABLED")

    if(ENGINE_ENABLE_LTO AND NOT ENGINE_IPO_SUPPORTED)
        message(WARNING
            "IPO/LTO is unsupported by this toolchain: ${ENGINE_IPO_ERROR}"
        )
    endif()
endif()

function(configure_engine_core target)
    target_include_directories(${target}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    target_compile_features(${target}
        PUBLIC cxx_std_23
    )

    if(ENGINE_USE_PEXT)
        target_compile_definitions(${target}
            PUBLIC USE_PEXT
        )

        if(ROOK_COMPRESSED)
            target_compile_definitions(${target}
                PUBLIC ATTACKS_ROOK_COMPRESSED
            )
        endif()

        if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_compile_options(${target}
                PUBLIC /arch:AVX2
            )

            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${target}
                    PUBLIC /clang:-mbmi2
                )
            endif()

        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            if(ENGINE_X86_ARCH)
                target_compile_options(${target}
                    PUBLIC "-march=${ENGINE_X86_ARCH}"
                )
            else()
                target_compile_options(${target}
                    PUBLIC -mbmi2
                )
            endif()
        endif()
    endif()
endfunction()

# Normal core: used by the GUI and tests.
add_library(EngineCore STATIC ${ENGINE_SRC})
configure_engine_core(EngineCore)

# Explicitly disable IPO for the normal core in every configuration.
set_property(
    TARGET EngineCore
    PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE
)

foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_property(
        TARGET EngineCore
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_${config} FALSE
    )
endforeach()


# Standalone engine executable.
add_executable(CheckspeareEngine src/main.cpp)

# Default to no IPO, unless explicitly enabled below.
set_property(
    TARGET CheckspeareEngine
    PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE
)

foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_property(
        TARGET CheckspeareEngine
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_${config} FALSE
    )
endforeach()


if(ENGINE_ENABLE_LTO AND ENGINE_IPO_SUPPORTED)

    # Separate core variant compiled with LTO.
    add_library(EngineCoreLTO STATIC ${ENGINE_SRC})
    configure_engine_core(EngineCoreLTO)

    # Explicitly disable IPO by default.
    set_property(
        TARGET EngineCoreLTO
        PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE
    )

    foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_property(
            TARGET EngineCoreLTO
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_${config} FALSE
        )
    endforeach()

    # Enable IPO only for optimized configurations.
    foreach(config IN ITEMS RELEASE RELWITHDEBINFO MINSIZEREL)

        set_property(
            TARGET EngineCoreLTO
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_${config} TRUE
        )

        set_property(
            TARGET CheckspeareEngine
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_${config} TRUE
        )

    endforeach()

    target_link_libraries(CheckspeareEngine
        PRIVATE EngineCoreLTO
    )

else()

    # Portable fallback when LTO is disabled or unsupported.
    target_link_libraries(CheckspeareEngine
        PRIVATE EngineCore
    )

endif()

# Compiler warnings.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(CheckspeareEngine PRIVATE /W4 /wd4100 /wd4189)
else()
    target_compile_options(CheckspeareEngine PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

message(STATUS "PEXT sliding attacks: ${ENGINE_USE_PEXT}")
message(STATUS "Compressed rook table: ${ROOK_COMPRESSED}")

if(APPLE)
    configure_portable_runtime(CheckspeareEngine)
else()
    configure_portable_runtime(CheckspeareEngine FULL_STATIC)
endif()

add_subdirectory(tests)