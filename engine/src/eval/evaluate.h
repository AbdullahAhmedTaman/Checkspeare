#pragma once
#include "board/position.h"
#include "board/types.h"

namespace Eval
{ // material values =  {pawn, knight, bishop, rook, queen, king}
    constexpr int MG_WEIGHT[PieceType::NONE] = {82, 337, 365, 477, 1025, 0};
    constexpr int EG_WEIGHT[PieceType::NONE] = {94, 281, 297, 512, 936, 0};

    void init_scores(Position& pos); // call it once after parsing FEN

    // Pawn Structure Penalties & Bonuses
    constexpr int DOUBLED_PAWN_PENALTY_MG = 12;
    constexpr int DOUBLED_PAWN_PENALTY_EG = 20;

    constexpr int ISOLATED_PAWN_PENALTY_MG = 15;
    constexpr int ISOLATED_PAWN_PENALTY_EG = 20;

    // Passed Pawn Bonus by Rank (Rank 0 and 7 are unused)
    constexpr int PASSED_PAWN_BONUS_MG[8] = {0, 5, 10, 20, 35, 60, 100, 0};
    constexpr int PASSED_PAWN_BONUS_EG[8] = {0, 10, 20, 40, 70, 110, 160, 0};

    // knight outpost bonus
    constexpr int KNIGHT_OUTPOST_MG = 20;
    constexpr int KNIGHT_OUTPOST_EG = 10;

    void init_pawn_masks();
    int evaluate(const Position& pos);
    Score evaluate_bishop_pair(const Position& pos);
    Score evaluate_king_shield(const Position& pos); // only evluates king pawn shield
    Score evaluate_pieces(const Position& pos);      // evaluates mobility and king attacking zones
    Score evaluate_pawns(const Position& pos);       // Pawn evaluation logic
    Score evaluate_outposts(const Position& pos);
} // namespace Eval