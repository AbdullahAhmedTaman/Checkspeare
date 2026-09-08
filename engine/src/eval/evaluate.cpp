#include "evaluate.h"
#include "bitboard.h"
#include "pst.h"

namespace Eval
{

    const int* PST_MG[6] = {PAWN_MG, KNIGHT_MG, BISHOP_MG, ROOK_MG, QUEEN_MG, KING_MG};

    const int* PST_EG[6] = {PAWN_EG, KNIGHT_EG, BISHOP_EG, ROOK_EG, QUEEN_EG, KING_EG};

    constexpr int GAME_PHASE_MAX = 24;

    constexpr PieceType ALL_PIECE_TYPES[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

    int evaluate(const Position& pos)
    {
        int mgScore = pos.mg_score[WHITE] - pos.mg_score[BLACK];
        int egScore = pos.eg_score[WHITE] - pos.eg_score[BLACK];

        int phase = pos.game_phase;
        if (phase > GAME_PHASE_MAX)
            phase = GAME_PHASE_MAX;

        int mgWeight = (phase * 256) / GAME_PHASE_MAX;
        int egWeight = 256 - mgWeight;

        int finalEval = ((mgScore * mgWeight) + (egScore * egWeight)) >> 8;
        return (pos.sideToMove == WHITE) ? finalEval : -finalEval;
    }

    void init_scores(Position& pos)
    {
        pos.mg_score[WHITE] = 0;
        pos.mg_score[BLACK] = 0;
        pos.eg_score[WHITE] = 0;
        pos.eg_score[BLACK] = 0;
        pos.game_phase = 0;

        for (int color = WHITE; color <= BLACK; ++color)
        {
            for (PieceType pt : ALL_PIECE_TYPES)
            {
                Bitboard bb = pos.pieces[color][pt];
                int count = popcount(bb);
                pos.game_phase += count * PHASE_WEIGHTS[pt];

                while (bb)
                {
                    Square sq = pop_lsb(bb);
                    Square pst_sq = (color == WHITE) ? mirror_square(sq) : sq;

                    pos.mg_score[color] += MG_WEIGHT[pt] + PST_MG[pt][pst_sq];
                    pos.eg_score[color] += EG_WEIGHT[pt] + PST_EG[pt][pst_sq];
                }
            }
        }
    }

} // namespace Eval