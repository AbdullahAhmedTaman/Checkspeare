#include "evaluate.h"
#include "bitboard.h"
#include "pst.h"
#include <algorithm>

namespace Eval
{
    constexpr int GAME_PHASE_MAX = 24;

    constexpr PieceType ALL_PIECE_TYPES[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

    int taper(const Score& score, const Position& pos)
    {
        int phase = std::min(pos.game_phase, GAME_PHASE_MAX);

        int mgWeight = (phase * 256) / GAME_PHASE_MAX;
        int egWeight = 256 - mgWeight;

        int finalEval = ((score.mg * mgWeight) + (score.eg * egWeight)) >> 8;
        return (pos.sideToMove == WHITE) ? finalEval : -finalEval;
    }

    int evaluate(const Position& pos)
    {
        Score score;
        score.mg = pos.mg_score[WHITE] - pos.mg_score[BLACK];
        score.eg = pos.eg_score[WHITE] - pos.eg_score[BLACK];

        return taper(score, pos);
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

                    pos.mg_score[color] += MG_WEIGHT[pt] + PST.mg[color][pt][sq];
                    pos.eg_score[color] += EG_WEIGHT[pt] + PST.eg[color][pt][sq];
                }
            }
        }
    }

} // namespace Eval