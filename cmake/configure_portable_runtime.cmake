# Per-platform behavior (executables only):
#   Windows + MSVC/clang-cl : no-op (the static CRT is handled elsewhere).
#   Windows + MinGW         : -static-libgcc -static-libstdc++ -static
#                             (only Windows system DLLs remain, opengl32 included).
#   macOS                   : static runtime is impossible; sets architectures
#                             and checks the deployment target instead.
#   Linux / other Unix      : -static-libgcc -static-libstdc++.
#                             glibc and libGL stay dynamic ON PURPOSE: OpenGL
#                             drivers are loaded with dlopen, which a fully
#                             static binary can't do.
#
# Usage:
#   set(IS_PRODUCTION_CONFIG "$<CONFIG:Production>")   # or pass CONFIG_CONDITION
#   configure_portable_runtime(executable)
#   configure_portable_runtime(executable UNIVERSAL_BINARY)       # macOS arm64+x86_64
#   configure_portable_runtime(executable CONFIG_CONDITION "$<CONFIG:Release>")
#   configure_portable_runtime(executable FULL_STATIC)            # Linux, see warning
#
# macOS note: set CMAKE_OSX_DEPLOYMENT_TARGET BEFORE the first project() call.

include_guard(GLOBAL)
include(CMakePushCheckState)
include(CheckCXXSourceCompiles)

function(configure_portable_runtime target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        "FULL_STATIC;UNIVERSAL_BINARY"
        "CONFIG_CONDITION"
        "")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "configure_portable_runtime: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT TARGET "${target}")
        message(FATAL_ERROR "configure_portable_runtime: '${target}' is not a target")
    endif()

    if(NOT CMAKE_CXX_COMPILER_LOADED)
        message(FATAL_ERROR "configure_portable_runtime: C++ must be an enabled language")
    endif()

    # Which configurations get the portable settings.
    if(ARG_CONFIG_CONDITION)
        set(_cond "${ARG_CONFIG_CONDITION}")
    elseif(DEFINED IS_PRODUCTION_CONFIG)
        set(_cond "${IS_PRODUCTION_CONFIG}")
    else()
        message(FATAL_ERROR
            "configure_portable_runtime: define IS_PRODUCTION_CONFIG or pass CONFIG_CONDITION")
    endif()

    # Link options only make sense on real executables.
    get_target_property(_imported "${target}" IMPORTED)
    get_target_property(_type "${target}" TYPE)
    if(_imported OR NOT _type STREQUAL "EXECUTABLE")
        message(WARNING
            "configure_portable_runtime: '${target}' is not a non-imported executable "
            "(type: ${_type}). Static runtime flags on shared/module targets can duplicate "
            "runtime state and break exceptions across library boundaries, so nothing was applied.")
        return()
    endif()

    # Apple
    # APPLE also covers upstream/Homebrew Clang (ID "Clang") and iOS toolchains.
    if(APPLE OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        if(ARG_FULL_STATIC)
            message(WARNING
                "configure_portable_runtime: FULL_STATIC is impossible on Apple platforms "
                "(no static libSystem). Ignoring.")
        endif()

        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            message(WARNING
                "configure_portable_runtime: CMAKE_OSX_DEPLOYMENT_TARGET is not set. The binary "
                "will require the macOS version of the machine that built it. Set it before "
                "project(), e.g. set(CMAKE_OSX_DEPLOYMENT_TARGET \"11.0\" CACHE STRING \"\").")
        endif()

        if(ARG_UNIVERSAL_BINARY)
            if(CMAKE_OSX_ARCHITECTURES)
                message(STATUS
                    "configure_portable_runtime: CMAKE_OSX_ARCHITECTURES already set "
                    "(${CMAKE_OSX_ARCHITECTURES}), leaving '${target}' as is")
            else()
                set_target_properties(${target} PROPERTIES OSX_ARCHITECTURES "arm64;x86_64")
            endif()
        endif()
        return()
    endif()

    # MSVC
    # MSVC and clang-cl: static CRT is handled elsewhere.
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        return()
    endif()

    # Only GNU-style command lines from here on (gcc, clang, MinGW, clang in GNU mode).
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
            OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
        message(STATUS
            "configure_portable_runtime: unsupported compiler "
            "'${CMAKE_CXX_COMPILER_ID}', no flags applied to '${target}'")
        return()
    endif()

    set(_libs ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
    set(_has_gcc   FALSE)
    set(_has_stdcxx FALSE)
    set(_has_libcxx FALSE)
    if("gcc" IN_LIST _libs OR "gcc_s" IN_LIST _libs)
        set(_has_gcc TRUE)
    endif()
    if("stdc++" IN_LIST _libs)
        set(_has_stdcxx TRUE)
    endif()
    if("c++" IN_LIST _libs)
        set(_has_libcxx TRUE)
    endif()

    if(_has_gcc)
        target_link_options(${target} PRIVATE "$<${_cond}:-static-libgcc>")
    endif()

    if(_has_stdcxx)
        target_link_options(${target} PRIVATE "$<${_cond}:-static-libstdc++>")
    elseif(_has_libcxx)
        # No driver-level static flag exists for libc++ (on MinGW, -static below
        # covers it). Elsewhere it needs libc++.a/libc++abi.a and a toolchain-
        # specific recipe, so it is left dynamic rather than guessed at.
        if(NOT WIN32)
            message(WARNING
                "configure_portable_runtime: libc++ detected for '${target}'. It stays "
                "dynamic, so users need libc++ installed. Either build with libstdc++ "
                "(-stdlib=libstdc++) or link libc++.a and libc++abi.a manually.")
        endif()
    endif()

    # Windows
    if(WIN32)
        # MinGW: -static also pulls in winpthread. opengl32 is a system DLL
        # via its import library, so it keeps working.
        target_link_options(${target} PRIVATE "$<${_cond}:-static>")
        return()
    endif()

    # Linux / Unix
    if(ARG_FULL_STATIC)
        message(WARNING
            "configure_portable_runtime: FULL_STATIC requested for '${target}'. A fully static "
            "binary cannot dlopen, so it will NOT be able to load OpenGL/Vulkan drivers. "
            "Use it only for programs without GPU or plugin loading.")

        if(NOT DEFINED PORTABLE_RUNTIME_CAN_LINK_STATIC)
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_LINK_OPTIONS "-static")
            check_cxx_source_compiles(
                "#include <iostream>\nint main() { std::cout << 1; return 0; }"
                PORTABLE_RUNTIME_CAN_LINK_STATIC)
            cmake_pop_check_state()
        endif()

        if(PORTABLE_RUNTIME_CAN_LINK_STATIC)
            target_link_options(${target} PRIVATE "$<${_cond}:-static>")
        else()
            message(WARNING
                "configure_portable_runtime: '-static' test link failed (libc.a / libstdc++.a "
                "missing?). Falling back to static libgcc/libstdc++ only.")
        endif()
    endif()
endfunction()