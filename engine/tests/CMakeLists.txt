add_executable(EngineTests
    position_tests.cpp
)

target_link_libraries(EngineTests
    PRIVATE
        ChessEngine
        Catch2::Catch2WithMain
)

include(Catch)

catch_discover_tests(EngineTests)