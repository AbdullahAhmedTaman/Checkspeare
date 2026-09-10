add_executable(EngineTests
legal_moves_tests.cpp
position_zobrist_tests.cpp
)

target_link_libraries(EngineTests
    PRIVATE
        ChessEngine
        Catch2::Catch2WithMain
)

target_include_directories(EngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

include(Catch)

catch_discover_tests(EngineTests)