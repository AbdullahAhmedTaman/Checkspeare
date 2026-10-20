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
    message(STATUS "IPO/LTO Engine: ENABLED")
else()
    message(STATUS "IPO/LTO Engine: NOT SUPPORTED")
    message(STATUS "IPO/LTO Engine reason: ${ipo_msg}")
endif()

add_library(EngineCore ${ENGINE_SRC})

target_include_directories(EngineCore
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(EngineCore PUBLIC cxx_std_23)

if(ENGINE_USE_PEXT)
    target_compile_definitions(EngineCore PUBLIC USE_PEXT)

    if(ROOK_COMPRESSED)
        target_compile_definitions(EngineCore PUBLIC ATTACKS_ROOK_COMPRESSED)
    endif()

    # BMI2 is needed only when PEXT is used, so the non-PEXT build stays
    # runnable on baseline x86-64 (and on non-x86 targets).
    # PUBLIC is intentional: attacks.h contains inline PEXT functions that
    # are compiled in EngineCore's consumers, so flags must propagate.
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # MSVC exposes _pext_u64 without a BMI2 flag; /arch:AVX2 only
        # allows AVX2 codegen. clang-cl does need BMI2 enabled explicitly.
        target_compile_options(EngineCore PUBLIC /arch:AVX2)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(EngineCore PUBLIC /clang:-mbmi2)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        if(ENGINE_X86_ARCH)
            target_compile_options(EngineCore PUBLIC -march=${ENGINE_X86_ARCH})
        else()
            target_compile_options(EngineCore PUBLIC -mbmi2)
        endif()
    endif()
endif()


add_executable(CheckspeareEngine src/main.cpp)

target_link_libraries(CheckspeareEngine PRIVATE EngineCore)

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