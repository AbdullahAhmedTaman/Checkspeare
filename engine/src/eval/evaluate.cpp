#include "evaluate.h"
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "types.h"
#include <algorithm>

namespace Eval
{
    constexpr int GAME_PHASE_MAX = 24;

    constexpr Bitboard NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;
    // position heuristics bonuses
    // bishop pair
    constexpr int BISHOP_PAIR_MG = 20;
    constexpr int BISHOP_PAIR_EG = 50;
    // king safety
    constexpr int PAWN_SHIELD_BONUS_MG = 15;
    constexpr int PAWN_SHIELD_BONUS_EG = 5;

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

    Score evaluate_king_safety(const Position& pos)
    {
        Score s;
        Bitboard w_king = pos.pieces[WHITE][KING];
        Bitboard b_king = pos.pieces[BLACK][KING];
        Bitboard w_pawns = pos.pieces[WHITE][PAWN];
        Bitboard b_pawns = pos.pieces[BLACK][PAWN];

        // pawn shield
        Bitboard w_shield_center = w_king << 8;
        Bitboard w_shield_left = (w_shield_center >> 1) & NOT_H_FILE;
        Bitboard w_shield_right = (w_shield_center << 1) & NOT_A_FILE;
        Bitboard w_shield_mask = w_shield_center | w_shield_left | w_shield_right;

        Bitboard b_shield_center = b_king >> 8;
        Bitboard b_shield_left = (b_shield_center >> 1) & NOT_H_FILE;
        Bitboard b_shield_right = (b_shield_center << 1) & NOT_A_FILE;
        Bitboard b_shield_mask = b_shield_center | b_shield_left | b_shield_right;

        int w_shield_count = popcount(w_pawns & w_shield_mask);
        int b_shield_count = popcount(b_pawns & b_shield_mask);

        s.mg[WHITE] += w_shield_count * PAWN_SHIELD_BONUS_MG;
        s.eg[WHITE] += w_shield_count * PAWN_SHIELD_BONUS_EG;

        s.mg[BLACK] += b_shield_count * PAWN_SHIELD_BONUS_MG;
        s.eg[BLACK] += b_shield_count * PAWN_SHIELD_BONUS_EG;

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
        score += evaluate_king_safety(pos);

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