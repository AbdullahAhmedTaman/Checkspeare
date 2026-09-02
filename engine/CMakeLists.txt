add_executable(ChessEngine)


file(GLOB_RECURSE BOARD_SRC "src/board/*.cpp")
file(GLOB_RECURSE SEARCH_SRC "src/search/*.cpp")
file(GLOB_RECURSE EVAL_SRC "src/eval/*.cpp")
file(GLOB_RECURSE UCI_SRC "src/uci/*.cpp")

target_sources(ChessEngine
    PRIVATE
        src/main.cpp
        ${BOARD_SRC}
        ${SEARCH_SRC}
        ${EVAL_SRC}
        ${UCI_SRC}
)

target_include_directories(ChessEngine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)