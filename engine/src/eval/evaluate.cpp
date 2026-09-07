#include "evaluate.h"
#include "pst.h"
#include "bitboard.h"

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

constexpr PieceType ALL_PIECE_TYPES[] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

int evaluate(const Position& pos)
{
    int white_mg = 0;
    int white_eg = 0;
    int black_mg = 0;
    int black_eg = 0;

    int gamePhase = 0;

    // calculation for white pieces
    for (PieceType pt : ALL_PIECE_TYPES)
    {
        Bitboard bb = pos.pieces[WHITE][pt];
        int count = popcount(bb);

        // calculate phase value
        gamePhase += count * PHASE_WEIGHTS[pt];

        // calculate material score 
        white_mg += count * MG_WEIGHT[pt];
        white_eg += count * EG_WEIGHT[pt];

        // calculate PST score for each piece of the specified type
        while (bb)
        {
            Square sq = pop_lsb(bb);
            Square pst_sq = mirror_square(sq);
            
            white_mg += PST_MG[pt][pst_sq];
            white_eg += PST_EG[pt][pst_sq];
        }
    }
    // calculation for black pieces
    for (PieceType pt : ALL_PIECE_TYPES)
    {
        Bitboard bb = pos.pieces[BLACK][pt];
        int count = popcount(bb);

        gamePhase += count * PHASE_WEIGHTS[pt];

        black_mg += count * MG_WEIGHT[pt];
        black_eg += count * EG_WEIGHT[pt];

        while (bb)
        {
            Square sq = pop_lsb(bb);
            
            black_mg += PST_MG[pt][sq];
            black_eg += PST_EG[pt][sq];
        }
    }
    
    if (gamePhase > GAME_PHASE_MAX)
        gamePhase = GAME_PHASE_MAX;

    int mgScore = white_mg - black_mg;
    int egScore = white_eg - black_eg;

    int mgWeight = (gamePhase * 256) / GAME_PHASE_MAX;
    int egWeight = 256 - mgWeight;

    int finalEval = ((mgScore * mgWeight) + (egScore * egWeight)) >> 8;

    if (pos.sideToMove == WHITE)
    {
        return finalEval;
    }
    else
    {
        return -finalEval;
    }
}

}