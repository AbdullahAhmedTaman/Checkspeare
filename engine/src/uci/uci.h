#pragma once
#include "board/position.h"
#include "search/search.h"
#include "board/types.h"
#include <string>

namespace UCI
{
    void loop();
    void send_search_info(const Search::SearchInfo& info);
    Move string_to_move(const std::string& move_str, Position& pos);
} // namespace UCI