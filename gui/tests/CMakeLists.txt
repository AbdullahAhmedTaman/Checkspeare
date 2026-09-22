# gui/tests/CMakeLists.txt
#
# Standalone unit tests for the GUI-side domain layer (San, Board).
# Does NOT link SFML or ImGui. Links ChessEngine for Position/MoveGen/etc.

# --- Paths relative to this file ---
set(GUI_SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(GUI_INC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(ENGINE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/include")

# --- San unit test ---
add_executable(SanTest
    SanTest.cpp
    ${GUI_SRC_DIR}/San.cpp
    ${GUI_SRC_DIR}/Board.cpp
)

target_include_directories(SanTest
    PRIVATE
        ${GUI_INC_DIR}
        ${ENGINE_INC_DIR}
)

# Link the engine library (it provides Position, MoveGen, attacks, zobrist).
# ChessEngine is defined in engine/CMakeLists.txt and already built.
target_link_libraries(SanTest
    PRIVATE
        ChessEngine
)

# Optional: register with CTest so `ctest` runs it.
add_test(NAME SanTest COMMAND SanTest)