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

configure_gnu_production_runtime(Checkspeare)

add_subdirectory(tests)
