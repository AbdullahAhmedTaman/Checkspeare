
file(GLOB_RECURSE BOARD_SRC "src/board/*.cpp")
file(GLOB_RECURSE SEARCH_SRC "src/search/*.cpp")
file(GLOB_RECURSE EVAL_SRC "src/eval/*.cpp")
file(GLOB_RECURSE UCI_SRC "src/uci/*.cpp")

add_library(ChessEngine
         ${BOARD_SRC}
         ${SEARCH_SRC}
         ${EVAL_SRC}
         ${UCI_SRC}
)

target_include_directories(ChessEngine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(Engine
         src/main.cpp
)   

target_link_libraries(Engine
    PRIVATE ChessEngine
)

add_subdirectory(tests)
