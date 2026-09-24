# gui/tests/CMakeLists.txt
#
# Standalone unit tests for the GUI-side domain layer (San, Board).
# Does NOT link SFML or ImGui. Links ChessEngine for Position/MoveGen/etc.

# --- Paths relative to this file ---
set(GUI_SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(GUI_INC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(ENGINE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include")

# --- San unit test ---
add_executable(EnginePlayerSelfGameTest
    EnginePlayerSelfGameTest.cpp
    ${GUI_SRC_DIR}/EnginePlayer.cpp
    ${GUI_SRC_DIR}/ChessGame.cpp
    ${GUI_SRC_DIR}/Board.cpp
    ${GUI_SRC_DIR}/San.cpp
    ${GUI_SRC_DIR}/ChessClock.cpp
    ${GUI_SRC_DIR}/UciEngine.cpp
    ${GUI_SRC_DIR}/Process_windows.cpp
    ${GUI_SRC_DIR}/Process_posix.cpp

)

target_include_directories(EnginePlayerSelfGameTest
    PRIVATE
        ${GUI_INC_DIR}
        ${ENGINE_INC_DIR}
)

# Link the engine library (it provides Position, MoveGen, attacks, zobrist).
# ChessEngine is defined in engine/CMakeLists.txt and already built.
target_link_libraries(EnginePlayerSelfGameTest
    PRIVATE
        ChessEngine
)

# ---- GameOverTest ----
add_executable(GameOverTest
    GameOverTest.cpp
    ${GUI_SRC_DIR}/ChessGame.cpp
    ${GUI_SRC_DIR}/Board.cpp
    ${GUI_SRC_DIR}/San.cpp
    ${GUI_SRC_DIR}/ChessClock.cpp
    ${GUI_SRC_DIR}/EnginePlayer.cpp
    ${GUI_SRC_DIR}/UciEngine.cpp
    ${GUI_SRC_DIR}/Process_windows.cpp
    ${GUI_SRC_DIR}/Process_posix.cpp
    ${GUI_SRC_DIR}/GuiGameController.cpp
    ${GUI_SRC_DIR}/BoardAnnotations.cpp
)
target_include_directories(GameOverTest PRIVATE
    ${GUI_INC_DIR}
    ${ENGINE_INC_DIR}
)
target_link_libraries(GameOverTest PRIVATE
    ChessEngine
    SFML::System
)
add_test(NAME GameOverTest COMMAND GameOverTest)