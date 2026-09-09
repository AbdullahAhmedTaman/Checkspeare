add_executable(EngineTests
    test_attacks.cpp
)

target_link_libraries(EngineTests
    PRIVATE
        ChessEngine
        Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(EngineTests)