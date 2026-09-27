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

add_executable(CheckSpeare
        src/main.cpp
)

target_link_libraries(CheckSpeare
        PRIVATE
        EngineCore
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(CheckSpeare PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR PRODUCTION_BUILD)
        if (WIN32)
         target_link_options(ChessGUI PRIVATE
          -static
          -static-libgcc
          -static-libstdc++
           )
        endif()
        
    endif()

elseif(MSVC)
    target_compile_options(CheckSpeare PRIVATE
            /W4
            /wd4100
            /wd4189
    )

    if(PRODUCTION_BUILD)
        set_property(TARGET CheckSpeare PROPERTY
                MSVC_RUNTIME_LIBRARY
                "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

add_subdirectory(tests)
