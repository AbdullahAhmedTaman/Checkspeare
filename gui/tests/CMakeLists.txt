# Standalone tests for GUI game, controller, and engine-player logic.

set(GUI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

if(WIN32)
    set(GUI_PROCESS_SOURCE "${GUI_SRC_DIR}/engine/Process_windows.cpp")
else()
    set(GUI_PROCESS_SOURCE "${GUI_SRC_DIR}/engine/Process_posix.cpp")
endif()

set(GUI_GAME_SOURCES
    "${GUI_SRC_DIR}/game/Board.cpp"
    "${GUI_SRC_DIR}/game/ChessGame.cpp"
    "${GUI_SRC_DIR}/game/ChessClock.cpp"
    "${GUI_SRC_DIR}/game/San.cpp"
)

set(GUI_ENGINE_PLAYER_SOURCES
    "${GUI_SRC_DIR}/engine/EnginePlayer.cpp"
    "${GUI_SRC_DIR}/engine/UciEngine.cpp"
    "${GUI_PROCESS_SOURCE}"
)

add_executable(ChessClockTest
    ChessClockTest.cpp
    "${GUI_SRC_DIR}/game/ChessClock.cpp"
)
target_link_libraries(ChessClockTest PRIVATE EngineCore)
add_test(NAME ChessClockTest COMMAND ChessClockTest)

add_executable(ChessGameTest
    ChessGameTest.cpp
    "${GUI_SRC_DIR}/game/Board.cpp"
    "${GUI_SRC_DIR}/game/ChessGame.cpp"
    "${GUI_SRC_DIR}/game/San.cpp"
)
target_link_libraries(ChessGameTest PRIVATE EngineCore)
add_test(NAME ChessGameTest COMMAND ChessGameTest)

add_executable(SanTest
    SanTest.cpp
    "${GUI_SRC_DIR}/game/Board.cpp"
    "${GUI_SRC_DIR}/game/San.cpp"
)
target_link_libraries(SanTest PRIVATE EngineCore)
add_test(NAME SanTest COMMAND SanTest)

add_executable(EnginePlayerDecideActionsTest
    EnginePlayerDecideActionsTest.cpp
    ${GUI_GAME_SOURCES}
    ${GUI_ENGINE_PLAYER_SOURCES}
)
target_link_libraries(EnginePlayerDecideActionsTest PRIVATE EngineCore)
add_test(NAME EnginePlayerDecideActionsTest COMMAND EnginePlayerDecideActionsTest)

add_executable(EnginePlayerSelfGameTest
    EnginePlayerSelfGameTest.cpp
    ${GUI_GAME_SOURCES}
    ${GUI_ENGINE_PLAYER_SOURCES}
)
target_link_libraries(EnginePlayerSelfGameTest PRIVATE EngineCore)

add_executable(GameOverTest
    GameOverTest.cpp
    ${GUI_GAME_SOURCES}
    ${GUI_ENGINE_PLAYER_SOURCES}
    "${GUI_SRC_DIR}/controller/GuiGameController.cpp"
    "${GUI_SRC_DIR}/ui/board/BoardAnnotations.cpp"
)
target_link_libraries(GameOverTest PRIVATE EngineCore SFML::System)
add_test(NAME GameOverTest COMMAND GameOverTest)