#include "evaluate.h"
#include "pst.h"

namespace Eval
{

const int* PST_MG[6] = {
    PAWN_MG, KNIGHT_MG, BISHOP_MG, ROOK_MG, QUEEN_MG, KING_MG
};

const int* PST_EG[6] = {
    PAWN_EG, KNIGHT_EG, BISHOP_EG, ROOK_EG, QUEEN_EG, KING_EG
};

constexpr int GAME_PHASE_MAX = 24;

// (Pawn=0, Knight=1, Bishop=1, Rook=2, Queen=4, King=0)
constexpr int PHASE_WEIGHTS[6] = {0, 1, 1, 2, 4, 0};

} 