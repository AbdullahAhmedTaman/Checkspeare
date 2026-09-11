#include "evaluate.h"
#include "attacks.h"
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "types.h"
#include <algorithm>


namespace
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
    constexpr int ATTACK_WEIGHTS[16] = {0, 0, 50, 75, 88, 94, 97, 99, 100, 100, 100, 100, 100, 100, 100, 100};
    constexpr int ATTACK_VALUE[6] = {0, 20, 20, 40, 80, 0};

    constexpr PieceType ALL_PIECE_TYPES[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

    inline Bitboard get_king_zone(Square king_sq, Color c)
    {
        Bitboard ring = king_attacks(king_sq) | square_bb(king_sq);
        Bitboard forward = (c == WHITE) ? (ring << 8) : (ring >> 8);
        return ring | forward;
    }

    int calculate_king_danger(const Position& pos, Color attacker, Bitboard enemy_king_zone)
    {
        int attackingPiecesCount = 0;
        int valueOfAttacks = 0;

        Bitboard occupancy = pos.occupancy[BOTH];

        Bitboard knights = pos.pieces[attacker][KNIGHT];
        while (knights)
        {
            Square sq = pop_lsb(knights);
            Bitboard hits = knight_attacks(sq) & enemy_king_zone;
            if (hits)
            {
                attackingPiecesCount++;
                valueOfAttacks += popcount(hits) * ATTACK_VALUE[KNIGHT];
            }
        }

        Bitboard bishops = pos.pieces[attacker][BISHOP];
        while (bishops)
        {
            Square sq = pop_lsb(bishops);
            Bitboard hits = bishop_attacks(occupancy, sq) & enemy_king_zone;
            if (hits)
            {
                attackingPiecesCount++;
                valueOfAttacks += popcount(hits) * ATTACK_VALUE[BISHOP];
            }
        }

        Bitboard rooks = pos.pieces[attacker][ROOK];
        while (rooks)
        {
            Square sq = pop_lsb(rooks);
            Bitboard hits = rook_attacks(occupancy, sq) & enemy_king_zone;
            if (hits)
            {
                attackingPiecesCount++;
                valueOfAttacks += popcount(hits) * ATTACK_VALUE[ROOK];
            }
        }

        Bitboard queens = pos.pieces[attacker][QUEEN];
        while (queens)
        {
            Square sq = pop_lsb(queens);
            Bitboard hits = queen_attacks(occupancy, sq) & enemy_king_zone;
            if (hits)
            {
                attackingPiecesCount++;
                valueOfAttacks += popcount(hits) * ATTACK_VALUE[QUEEN];
            }
        }

        attackingPiecesCount = std::min(attackingPiecesCount, 15);

        // Toga Formula: value * weight[count] / 100
        return (valueOfAttacks * ATTACK_WEIGHTS[attackingPiecesCount]) / 100;
    }
} // namespace

namespace Eval
{
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

        // attacks on king --- !Danger: this could slow down the evaluation significantly if not optimized properly (use magic
        // bitboards).
        Square w_king_sq = lsb(w_king);
        Square b_king_sq = lsb(b_king);

        Bitboard wk_ring = get_king_zone(w_king_sq, WHITE);
        Bitboard bk_ring = get_king_zone(b_king_sq, BLACK);

        int w_danger = calculate_king_danger(pos, BLACK, wk_ring);
        s.mg[WHITE] -= w_danger;

        int b_danger = calculate_king_danger(pos, WHITE, bk_ring);
        s.mg[BLACK] -= b_danger;

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