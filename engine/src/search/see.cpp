#include "see.h"
#include "board/attacks.h"
#include "board/bitboard.h"

namespace Search
{
    constexpr int SEE_VALUES[] = {100, 300, 300, 500, 900, 20000, 0};

    bool see_ge(const Position& pos, Move move, int threshold)
    {
        
        if (move.is_castle() || move.is_null())
            return threshold <= 0;

        const Square from = move.from();
        const Square to = move.to();
        const Color us = pos.sideToMove;
        const Color them = static_cast<Color>(us ^ 1);

        const PieceType captured_pt = move.is_ep() ? PAWN : type_of(pos.board[to]);
        int swap_value = SEE_VALUES[captured_pt] - threshold;
        if (swap_value < 0)
            return false;

        const PieceType attacker_pt = type_of(pos.board[from]);
        swap_value = SEE_VALUES[attacker_pt] - swap_value;
        if (swap_value <= 0)
            return true;

       
        Bitboard occ = (pos.occupancy[BOTH] ^ square_bb(from)) | square_bb(to);
        if (move.is_ep())
        {
            occ ^= square_bb(square_behind(to, us));
        }

        const Bitboard diag_sliders =
            pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP] | pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN];
        const Bitboard orth_sliders =
            pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK] | pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN];

       
        Bitboard attackers = ((pawn_attacks(WHITE, to) & pos.pieces[BLACK][PAWN]) |
                              (pawn_attacks(BLACK, to) & pos.pieces[WHITE][PAWN]) |
                              (knight_attacks(to) & (pos.pieces[WHITE][KNIGHT] | pos.pieces[BLACK][KNIGHT])) |
                              (bishop_attacks(occ, to) & diag_sliders) |
                              (rook_attacks(occ, to) & orth_sliders) |
                              (king_attacks(to) & (pos.pieces[WHITE][KING] | pos.pieces[BLACK][KING]))) &
                             ~square_bb(to);

        Color stm = them;

        while (true)
        {
            attackers &= occ;
            const Bitboard stm_attackers = attackers & pos.occupancy[stm];
            if (!stm_attackers)
                break;

            PieceType pt = PAWN;
            Bitboard bb = 0;
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
            else
            {
                bb = stm_attackers & pos.pieces[stm][KING];
                pt = KING;
            }

            occ ^= square_bb(lsb(bb));

            if (pt == PAWN || pt == BISHOP || pt == QUEEN)
                attackers |= (bishop_attacks(occ, to) & diag_sliders) & ~square_bb(to);
            if (pt == ROOK || pt == QUEEN)
                attackers |= (rook_attacks(occ, to) & orth_sliders) & ~square_bb(to);

            stm = static_cast<Color>(stm ^ 1);
            swap_value = SEE_VALUES[pt] - swap_value;

          
            if (swap_value < (stm != us ? 1 : 0))
            {
                break;
            }
        }

        return stm != us;
        
    }
} 