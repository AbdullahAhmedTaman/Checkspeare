#pragma once
#include "position.h"
#include "types.h"
#include <string>

namespace UCI
{
    void loop();
    Move string_to_move(const std::string& move_str, Position& pos);
} // namespace UCI