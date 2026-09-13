#include "evaluate.h"
#include "attacks.h"
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "types.h"
#include <algorithm>

namespace Eval
{
    constexpr int GAME_PHASE_MAX = 24;

    constexpr Bitboard FILE_A = 0x0101010101010101ULL;
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

    // piece mobility
    constexpr int MOB_KNIGHT_MG = 4;
    constexpr int MOB_KNIGHT_EG = 4;
    constexpr int MOB_BISHOP_MG = 5;
    constexpr int MOB_BISHOP_EG = 5;
    constexpr int MOB_ROOK_MG = 2;
    constexpr int MOB_ROOK_EG = 4;
    constexpr int MOB_QUEEN_MG = 1;
    constexpr int MOB_QUEEN_EG = 2;

    constexpr PieceType ALL_PIECE_TYPES[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

    // Pawn Structure Masks
    Bitboard FILE_MASKS[8];
    Bitboard ISOLATED_MASKS[8];
    Bitboard WHITE_PASSED_MASKS[64];
    Bitboard BLACK_PASSED_MASKS[64];
    // run once in FEN 
    void init_pawn_masks() 
    {
        // 1. Build File Masks
        for (int col = 0; col < 8; ++col)
        {
            FILE_MASKS[col] = FILE_A << col;
        }

        // 2. Build Isolated Pawn Masks
        for (int col = 0; col < 8; ++col)
        {
            Bitboard left_file  = (FILE_MASKS[col] >> 1) & NOT_H_FILE;
            Bitboard right_file = (FILE_MASKS[col] << 1) & NOT_A_FILE;
            
            ISOLATED_MASKS[col] = left_file | right_file;
        }

        // 3. Build Passed Pawn Masks
        for (int sq = 0; sq < 64; ++sq)
        {
            int col = sq % 8;
            int row = sq / 8;

            Bitboard front_cols = FILE_MASKS[col] | ISOLATED_MASKS[col];

            Bitboard white_ranks = (row < 7) ? (~0ULL << (8 * (row + 1))) : 0ULL;

            Bitboard black_ranks = ~(~0ULL << (8 * row));

            WHITE_PASSED_MASKS[sq] = front_cols & white_ranks;
            BLACK_PASSED_MASKS[sq] = front_cols & black_ranks;
        }
    }

    inline Bitboard get_king_zone(Square king_sq, Color c)
    {
        Bitboard ring = king_attacks(king_sq) | square_bb(king_sq);
        Bitboard forward = (c == WHITE) ? (ring << 8) : (ring >> 8);
        return ring | forward;
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

    Score evaluate_king_shield(const Position& pos)
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

    Score evaluate_pawns(const Position& pos)
    {
        Score s;
        Bitboard wPawns = pos.pieces[WHITE][PAWN];
        Bitboard bPawns = pos.pieces[BLACK][PAWN];

        for (int col = 0; col < 8; ++col)
        {
            Bitboard wColPawns = wPawns & FILE_MASKS[col];
            Bitboard bColPawns = bPawns & FILE_MASKS[col];

            // Doubled Pawns Check
            int wCount = popcount(wColPawns);
            if (wCount > 1)
            {
                s.mg[WHITE] -= (wCount - 1) * DOUBLED_PAWN_PENALTY_MG;
                s.eg[WHITE] -= (wCount - 1) * DOUBLED_PAWN_PENALTY_EG;
            }

            int bCount = popcount(bColPawns);
            if (bCount > 1)
            {
                s.mg[BLACK] -= (bCount - 1) * DOUBLED_PAWN_PENALTY_MG;
                s.eg[BLACK] -= (bCount - 1) * DOUBLED_PAWN_PENALTY_EG;
            }

            // Isolated Pawns Check
            if (wColPawns && !(wPawns & ISOLATED_MASKS[col]))
            {
                s.mg[WHITE] -= wCount * ISOLATED_PAWN_PENALTY_MG;
                s.eg[WHITE] -= wCount * ISOLATED_PAWN_PENALTY_EG;
            }

            if (bColPawns && !(bPawns & ISOLATED_MASKS[col]))
            {
                s.mg[BLACK] -= bCount * ISOLATED_PAWN_PENALTY_MG;
                s.eg[BLACK] -= bCount * ISOLATED_PAWN_PENALTY_EG;
            }
        }

        // Passed Pawns Check for White
        Bitboard tempW = wPawns;
        while (tempW)
        {
            Square sq = pop_lsb(tempW);
            if (!(WHITE_PASSED_MASKS[sq] & bPawns))
            {
                int row = sq / 8;
                s.mg[WHITE] += PASSED_PAWN_BONUS_MG[row];
                s.eg[WHITE] += PASSED_PAWN_BONUS_EG[row];
            }
        }

        // Passed Pawns Check for Black
        Bitboard tempB = bPawns;
        while (tempB)
        {
            Square sq = pop_lsb(tempB);
            if (!(BLACK_PASSED_MASKS[sq] & wPawns))
            {
                int row = 7 - (sq / 8); // Perspective relative rank
                s.mg[BLACK] += PASSED_PAWN_BONUS_MG[row];
                s.eg[BLACK] += PASSED_PAWN_BONUS_EG[row];
            }
        }

        return s;
    }

    Score evaluate_pieces(const Position& pos)
    {
        Score s;
        Bitboard occupancy = pos.occupancy[BOTH];

        Square w_king_sq = pos.king_square(WHITE);
        Square b_king_sq = pos.king_square(BLACK);
        Bitboard king_zones[2] = {get_king_zone(w_king_sq, WHITE), get_king_zone(b_king_sq, BLACK)};

        Bitboard w_pawns = pos.pieces[WHITE][PAWN];
        Bitboard b_pawns = pos.pieces[BLACK][PAWN];

        Bitboard w_pawn_attacks = ((w_pawns << 9) & NOT_A_FILE) | ((w_pawns << 7) & NOT_H_FILE);
        Bitboard b_pawn_attacks = ((b_pawns >> 7) & NOT_A_FILE) | ((b_pawns >> 9) & NOT_H_FILE);
        Bitboard pawn_attacks[2] = {w_pawn_attacks, b_pawn_attacks};

        int attackers_count[2] = {0, 0};
        int attack_value[2] = {0, 0};

        for (int c = WHITE; c <= BLACK; ++c)
        {
            Color them = static_cast<Color>(c ^ 1);

            Bitboard safe_squares = ~pos.occupancy[c] & ~pawn_attacks[them];
            Bitboard enemy_zone = king_zones[them];

            Bitboard knight = pos.pieces[c][KNIGHT];
            while (knight)
            {
                Square sq = pop_lsb(knight);
                Bitboard attacks = knight_attacks(sq);

                int mob = popcount(attacks & safe_squares);
                s.mg[c] += mob * MOB_KNIGHT_MG;
                s.eg[c] += mob * MOB_KNIGHT_EG;

                Bitboard hits = attacks & enemy_zone;
                if (hits)
                {
                    attackers_count[c]++;
                    attack_value[c] += popcount(hits) * ATTACK_VALUE[KNIGHT];
                }
            }

            Bitboard bishops = pos.pieces[c][BISHOP];
            while (bishops)
            {
                Square sq = pop_lsb(bishops);
                Bitboard attacks = bishop_attacks(occupancy, sq);

                int mob = popcount(attacks & safe_squares);
                s.mg[c] += mob * MOB_BISHOP_MG;
                s.eg[c] += mob * MOB_BISHOP_EG;

                Bitboard hits = attacks & enemy_zone;
                if (hits)
                {
                    attackers_count[c]++;
                    attack_value[c] += popcount(hits) * ATTACK_VALUE[BISHOP];
                }
            }

            Bitboard rooks = pos.pieces[c][ROOK];
            while (rooks)
            {
                Square sq = pop_lsb(rooks);
                Bitboard attacks = rook_attacks(occupancy, sq);

                int mob = popcount(attacks & safe_squares);
                s.mg[c] += mob * MOB_ROOK_MG;
                s.eg[c] += mob * MOB_ROOK_EG;

                Bitboard hits = attacks & enemy_zone;
                if (hits)
                {
                    attackers_count[c]++;
                    attack_value[c] += popcount(hits) * ATTACK_VALUE[ROOK];
                }
            }

            Bitboard queens = pos.pieces[c][QUEEN];
            while (queens)
            {
                Square sq = pop_lsb(queens);
                Bitboard attacks = queen_attacks(occupancy, sq);

                int mob = popcount(attacks & safe_squares);
                s.mg[c] += mob * MOB_QUEEN_MG;
                s.eg[c] += mob * MOB_QUEEN_EG;

                Bitboard hits = attacks & enemy_zone;
                if (hits)
                {
                    attackers_count[c]++;
                    attack_value[c] += popcount(hits) * ATTACK_VALUE[QUEEN];
                }
            }
        }

        // Toga Formula: value * weight[count] / 100
        attackers_count[WHITE] = std::min(attackers_count[WHITE], 15);
        int w_danger = (attack_value[WHITE] * ATTACK_WEIGHTS[attackers_count[WHITE]]) / 100;
        s.mg[BLACK] -= w_danger;

        attackers_count[BLACK] = std::min(attackers_count[BLACK], 15);
        int b_danger = (attack_value[BLACK] * ATTACK_WEIGHTS[attackers_count[BLACK]]) / 100;
        s.mg[WHITE] -= b_danger;

        return s;
    }

    int taper(int mg_total, int eg_total, const Position& pos)
    {
        int phase = std::min(pos.game_phase, GAME_PHASE_MAX);

        int mgWeight = (phase * 256) / GAME_PHASE_MAX;
        int egWeight = 256 - mgWeight;

        int finalEval = ((mg_total * mgWeight) + (eg_total * egWeight)) >> 8;
        return (pos.sideToMove == WHITE) ? finalEval : -finalEval;
    }

    int evaluate(const Position& pos)
    {
        Score score;
        score.mg[WHITE] = pos.mg_score[WHITE];
        score.mg[BLACK] = pos.mg_score[BLACK];
        score.eg[WHITE] = pos.eg_score[WHITE];
        score.eg[BLACK] = pos.eg_score[BLACK];

        score += evaluate_bishop_pair(pos);
        score += evaluate_king_shield(pos);
        score += evaluate_pieces(pos);
        score += evaluate_pawns(pos); 

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
