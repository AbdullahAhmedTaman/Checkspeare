cmake_minimum_required(VERSION 3.10)
project(ChessEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -Wextra")

include_directories(src)

file(GLOB_RECURSE BOARD_SRC "src/board/*.cpp")
file(GLOB_RECURSE SEARCH_SRC "src/search/*.cpp")
file(GLOB_RECURSE EVAL_SRC "src/eval/*.cpp")
file(GLOB_RECURSE UCI_SRC "src/uci/*.cpp")

add_executable(engine
    src/main.cpp
    ${BOARD_SRC}
    ${SEARCH_SRC}
    ${EVAL_SRC}
    ${UCI_SRC}
)
