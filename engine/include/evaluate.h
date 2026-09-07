#pragma once
#include "position.h"
#include "types.h"

namespace Eval
{ // material values =  {pawn, knight, bishop, rook, queen, king}
    constexpr int MG_WEIGHT[PieceType::NONE] = {82, 337, 365, 477, 1025, 0};
    constexpr int EG_WEIGHT[PieceType::NONE] = {94, 281, 297, 512, 936, 0};

    // Pawn Structure Penalties & Bonuses
    constexpr int DOUBLED_PAWN_PENALTY_MG = 12;
    constexpr int DOUBLED_PAWN_PENALTY_EG = 20;

    constexpr int ISOLATED_PAWN_PENALTY_MG = 15;
    constexpr int ISOLATED_PAWN_PENALTY_EG = 20;
    // bonus from rank 0 to 7 
    constexpr int PASSED_PAWN_BONUS_MG[8] = { 0,  5, 10, 20, 35,  60, 100, 0 };
    constexpr int PASSED_PAWN_BONUS_EG[8] = { 0, 10, 20, 40, 70, 110, 160, 0 };

    // Function to initialize pawn masks at engine startup
    void init_pawn_masks();

    int evaluate(const Position& pos);
} // namespace Eval