add_executable(EngineTests
    ${CMAKE_CURRENT_SOURCE_DIR}/perft/perft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/perft/perft_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/position_zobrist_tests.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/test-search-v3.cpp
)

target_link_libraries(EngineTests
    PRIVATE
        EngineCore
        Catch2::Catch2WithMain
)

target_include_directories(EngineTests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/perft
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

include(Catch)
catch_discover_tests(EngineTests)