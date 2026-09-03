#pragma once
#include "position.h"
#include "types.h"

namespace Eval
{ // material values =  {pawn, knight, bishop, rook, queen, king}
    constexpr int MATERIAL_VALUES[PieceType::NONE] = {100, 320, 330, 500, 900, 20000};
    int evaluate(const Position& pos);
} // namespace Eval