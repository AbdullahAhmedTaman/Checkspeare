#include "evaluate.h"
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "types.h"
#include <algorithm>

namespace Eval
{
    constexpr int GAME_PHASE_MAX = 24;
    // position heuristics bonuses
    // bishop pair
    constexpr int BISHOP_PAIR_MG = 20;
    constexpr int BISHOP_PAIR_EG = 50;

    constexpr PieceType ALL_PIECE_TYPES[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

    int taper(int mg_total, int eg_total, const Position& pos)
    {
        int phase = std::min(pos.game_phase, GAME_PHASE_MAX);

        int mgWeight = (phase * 256) / GAME_PHASE_MAX;
        int egWeight = 256 - mgWeight;

        int finalEval = ((mg_total * mgWeight) + (eg_total * egWeight)) >> 8;
        return (pos.sideToMove == WHITE) ? finalEval : -finalEval;
    }

    Score evaluate_bishop_pair(const Position& pos)
    {
        Score s;

        for (int c = WHITE; c <= BLACK; ++c)
        {
            if (popcount(pos.pieces[c][BISHOP]) >= 2)
            {
                s.mg[c] += BISHOP_PAIR_MG;
                s.eg[c] += BISHOP_PAIR_EG;
            }
        }

        return s;
    }

    int evaluate(const Position& pos)
    {
        Score score;
        score.mg[WHITE] = pos.mg_score[WHITE];
        score.mg[BLACK] = pos.mg_score[BLACK];
        score.eg[WHITE] = pos.eg_score[WHITE];
        score.eg[BLACK] = pos.eg_score[BLACK];

        score += evaluate_bishop_pair(pos);

        int mg_total = score.mg[WHITE] - score.mg[BLACK];
        int eg_total = score.eg[WHITE] - score.eg[BLACK];
        return taper(mg_total, eg_total, pos);
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