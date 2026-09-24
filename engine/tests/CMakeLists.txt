add_executable(EngineTests
    ${CMAKE_CURRENT_SOURCE_DIR}/perft/perft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/perft/perft_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/position_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_search.cpp
)

# Force GCC runtime libraries to link statically on Windows
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(EngineTests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(EngineTests
    PRIVATE
        ChessEngine
        Catch2::Catch2WithMain
)

target_include_directories(EngineTests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/perft
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

include(Catch)
catch_discover_tests(EngineTests)