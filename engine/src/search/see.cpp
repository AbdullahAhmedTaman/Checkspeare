#include "see.h"
#include "board/attacks.h"
#include "board/bitboard.h"

namespace Search
{
    constexpr int SEE_VALUES[] = {100, 300, 300, 500, 900, 20000, 0};

    bool see_ge(const Position& pos, Move move, int threshold)
    {
        const Square from = move.from();
        const Square to = move.to();

        int swap_value = SEE_VALUES[type_of(pos.board[to])] - threshold;
        if (swap_value < 0)
            return false;

        swap_value = SEE_VALUES[type_of(pos.board[from])] - swap_value;
        if (swap_value <= 0)
            return true;

        Bitboard occ = pos.occupancy[BOTH] ^ square_bb(from);
        Color us = pos.sideToMove;
        Color them = static_cast<Color>(us ^ 1);

        Bitboard attackers =
            (pawn_attacks(WHITE, to) & pos.pieces[BLACK][PAWN]) | (pawn_attacks(BLACK, to) & pos.pieces[WHITE][PAWN]) |
            (knight_attacks(to) & (pos.pieces[WHITE][KNIGHT] | pos.pieces[BLACK][KNIGHT])) |
            (bishop_attacks(occ, to) &
             (pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP] | pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN])) |
            (rook_attacks(occ, to) &
             (pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK] | pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN])) |
            (king_attacks(to) & (pos.pieces[WHITE][KING] | pos.pieces[BLACK][KING]));

        Color stm = them;

        while (true)
        {
            attackers &= occ;
            Bitboard stm_attackers = attackers & pos.occupancy[stm];

            if (!stm_attackers)
                break;

            PieceType pt = PAWN;
            Bitboard bb;
            if ((bb = stm_attackers & pos.pieces[stm][PAWN]))
                pt = PAWN;
            else if ((bb = stm_attackers & pos.pieces[stm][KNIGHT]))
                pt = KNIGHT;
            else if ((bb = stm_attackers & pos.pieces[stm][BISHOP]))
                pt = BISHOP;
            else if ((bb = stm_attackers & pos.pieces[stm][ROOK]))
                pt = ROOK;
            else if ((bb = stm_attackers & pos.pieces[stm][QUEEN]))
                pt = QUEEN;
            else if ((bb = stm_attackers & pos.pieces[stm][KING]))
                pt = KING;

            occ ^= square_bb(lsb(bb));

            if (pt == PAWN || pt == BISHOP || pt == QUEEN)
                attackers |= bishop_attacks(occ, to) & (pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP] |
                                                        pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]);
            if (pt == ROOK || pt == QUEEN)
                attackers |= rook_attacks(occ, to) & (pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK] |
                                                      pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]);

            stm = static_cast<Color>(stm ^ 1);

            swap_value = SEE_VALUES[pt] - swap_value;
            if (swap_value < 1)
            {
                stm = static_cast<Color>(stm ^ 1);
                break;
            }
        }

        return stm != us;
    }
} // namespace Search