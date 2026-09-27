#pragma once
#include "board/position.h"
#include "board/types.h"

namespace Eval
{ // material values =  {pawn, knight, bishop, rook, queen, king}
    constexpr int MG_WEIGHT[PieceType::NONE] = {82, 337, 365, 477, 1025, 0};
    constexpr int EG_WEIGHT[PieceType::NONE] = {94, 281, 297, 512, 936, 0};

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

    /**
     * @brief generates the initial evaluation score for a given position
     * @attention call it once after parsing FEN!
     */
    void init_scores(Position& pos);

    /**
     * @brief generate masks for pawn evaluation
     * @attention call it once after parsing FEN!
     */
    void init_pawn_masks();

    /**
     * @brief main evaluation (HCE) function.
     * It compines all the implemented heuristics and applies Taper evaluation to them.
     */
    int evaluate(const Position& pos);

    /**
     * @brief give evaluation bonuses for bishop pair.
     * @attention this function only accounts for number of bishops a color have. It doesn't take bishop type into consideration.
     */
    Score evaluate_bishop_pair(const Position& pos);

    /**
     * @brief evaluates king pawn shield.
     */
    Score evaluate_king_shield(const Position& pos);

    /**
     * @brief evaluates mobility and king attacking zones.
     */
    Score evaluate_pieces(const Position& pos);

    /**
     * @brief evaluates douple, isolated, and passed pawns.
     * gives penalty for doupled and isolated pawns and bonus for passed pawn.
     */
    Score evaluate_pawns(const Position& pos);

    /**
     * @brief gives bonuses for knights on an output square (usually center of the board)
     */
    Score evaluate_outposts(const Position& pos);
} // namespace Eval