#include "movegen.h"
#include "attacks.h"
#include "bitboard.h"
#include "types.h"

namespace
{
    constexpr uint8_t WHITE_OO = 1 << 0;
    constexpr uint8_t WHITE_OOO = 1 << 1;
    constexpr uint8_t BLACK_OO = 1 << 2;
    constexpr uint8_t BLACK_OOO = 1 << 3;

    /**
     * @brief Adds non-pawn moves represented by an attack bitboard.
     *
     * The caller provides the attack mask before own-piece filtering.
     */
    inline void add_piece_moves(Bitboard attacks, Square from, Bitboard ownPieces, Bitboard enemies, Bitboard occupancy,
                                MoveList& list)
    {
        attacks &= ~ownPieces;

        Bitboard captures = attacks & enemies;
        Bitboard quiets = attacks & ~occupancy;

        while (captures)
        {
            const Square to = pop_lsb(captures);
            list.add(Move(from, to, CAPTURE));
        }

        while (quiets)
        {
            const Square to = pop_lsb(quiets);
            list.add(Move(from, to, QUIET_MOVE));
        }
    }

    template <Color Us>
    void add_pawn_moves_impl(const Position& pos, MoveList& list)
    {
        constexpr Color Them = static_cast<Color>(Us ^ 1);

        constexpr int PushDir = (Us == WHITE) ? 8 : -8;
        constexpr int PushDirNE = (Us == WHITE) ? 9 : -7;
        constexpr int PushDirNW = (Us == WHITE) ? 7 : -9;

        constexpr Bitboard StartRank = (Us == WHITE) ? RANK_2 : RANK_7;
        constexpr Bitboard PromoFrom = (Us == WHITE) ? RANK_7 : RANK_2;

        constexpr auto shiftUp = (Us == WHITE) ? shift_north : shift_south;

        constexpr auto shiftUpNE = (Us == WHITE) ? shift_north_east : shift_south_east;

        constexpr auto shiftUpNW = (Us == WHITE) ? shift_north_west : shift_south_west;

        const Bitboard pawns = pos.pieces[Us][PAWN];
        const Bitboard occupancy = pos.occupancy[BOTH];
        const Bitboard empty = ~occupancy;
        const Bitboard enemies = pos.occupancy[Them];

        const Bitboard promotingPawns = pawns & PromoFrom;
        const Bitboard regularPawns = pawns & ~PromoFrom;

        // Single pushes that do not promote.
        {
            Bitboard bb = shiftUp(regularPawns) & empty;

            while (bb)
            {
                const Square to = pop_lsb(bb);
                const Square from = static_cast<Square>(to - PushDir);

                list.add(Move(from, to, QUIET_MOVE));
            }
        }

        // Single pushes that promote.
        {
            Bitboard bb = shiftUp(promotingPawns) & empty;

            while (bb)
            {
                const Square to = pop_lsb(bb);
                const Square from = static_cast<Square>(to - PushDir);

                list.add(Move(from, to, QUEEN_PROMO));
                list.add(Move(from, to, ROOK_PROMO));
                list.add(Move(from, to, BISHOP_PROMO));
                list.add(Move(from, to, KNIGHT_PROMO));
            }
        }

        // Double pushes require both the intermediate and destination squares to be empty.
        {
            const Bitboard fromStart = pawns & StartRank;
            const Bitboard mid = shiftUp(fromStart) & empty;
            Bitboard dest = shiftUp(mid) & empty;

            while (dest)
            {
                const Square to = pop_lsb(dest);
                const Square from = static_cast<Square>(to - 2 * PushDir);

                list.add(Move(from, to, DOUBLE_PAWN_PUSH));
            }
        }

        // Captures that do not promote.
        {
            Bitboard ne = shiftUpNE(regularPawns) & enemies;
            Bitboard nw = shiftUpNW(regularPawns) & enemies;

            while (ne)
            {
                const Square to = pop_lsb(ne);
                const Square from = static_cast<Square>(to - PushDirNE);

                list.add(Move(from, to, CAPTURE));
            }

            while (nw)
            {
                const Square to = pop_lsb(nw);
                const Square from = static_cast<Square>(to - PushDirNW);

                list.add(Move(from, to, CAPTURE));
            }
        }

        // Captures that promote.
        {
            Bitboard ne = shiftUpNE(promotingPawns) & enemies;
            Bitboard nw = shiftUpNW(promotingPawns) & enemies;

            while (ne)
            {
                const Square to = pop_lsb(ne);
                const Square from = static_cast<Square>(to - PushDirNE);

                list.add(Move(from, to, QUEEN_PROMO_CAP));
                list.add(Move(from, to, ROOK_PROMO_CAP));
                list.add(Move(from, to, BISHOP_PROMO_CAP));
                list.add(Move(from, to, KNIGHT_PROMO_CAP));
            }

            while (nw)
            {
                const Square to = pop_lsb(nw);
                const Square from = static_cast<Square>(to - PushDirNW);

                list.add(Move(from, to, QUEEN_PROMO_CAP));
                list.add(Move(from, to, ROOK_PROMO_CAP));
                list.add(Move(from, to, BISHOP_PROMO_CAP));
                list.add(Move(from, to, KNIGHT_PROMO_CAP));
            }
        }

        // EP attackers are found by reversing the pawn attack relation.
        if (pos.enPassantSquare != SQ_NONE)
        {
            Bitboard attackers = pawn_attacks(Them, pos.enPassantSquare) & pawns;

            while (attackers)
            {
                const Square from = pop_lsb(attackers);

                list.add(Move(from, pos.enPassantSquare, EP_CAPTURE));
            }
        }
    }

    void add_pawn_moves(const Position& pos, MoveList& list)
    {
        if (pos.sideToMove == WHITE)
            add_pawn_moves_impl<WHITE>(pos, list);
        else
            add_pawn_moves_impl<BLACK>(pos, list);
    }

    void add_knight_moves(const Position& pos, MoveList& list)
    {
        const Color us = pos.sideToMove;
        const Color them = static_cast<Color>(us ^ 1);

        const Bitboard ownPieces = pos.occupancy[us];
        const Bitboard enemies = pos.occupancy[them];
        const Bitboard occupancy = pos.occupancy[BOTH];

        Bitboard knights = pos.pieces[us][KNIGHT];

        while (knights)
        {
            const Square from = pop_lsb(knights);

            add_piece_moves(knight_attacks(from), from, ownPieces, enemies, occupancy, list);
        }
    }

    template <typename AttackFn>
    void add_slider_piece_moves(Bitboard pieces, Bitboard ownPieces, Bitboard enemies, Bitboard occupancy, MoveList& list,
                                AttackFn attackFn)
    {
        while (pieces)
        {
            const Square from = pop_lsb(pieces);

            add_piece_moves(attackFn(occupancy, from), from, ownPieces, enemies, occupancy, list);
        }
    }

    void add_slider_moves(const Position& pos, MoveList& list)
    {
        const Color us = pos.sideToMove;
        const Color them = static_cast<Color>(us ^ 1);

        const Bitboard ownPieces = pos.occupancy[us];
        const Bitboard enemies = pos.occupancy[them];
        const Bitboard occupancy = pos.occupancy[BOTH];

        add_slider_piece_moves(pos.pieces[us][ROOK], ownPieces, enemies, occupancy, list, rook_attacks);

        add_slider_piece_moves(pos.pieces[us][BISHOP], ownPieces, enemies, occupancy, list, bishop_attacks);

        add_slider_piece_moves(pos.pieces[us][QUEEN], ownPieces, enemies, occupancy, list, queen_attacks);
    }

    void add_king_moves(const Position& pos, MoveList& list)
    {
        const Color us = pos.sideToMove;
        const Color them = static_cast<Color>(us ^ 1);

        const Bitboard ownPieces = pos.occupancy[us];
        const Bitboard enemies = pos.occupancy[them];
        const Bitboard occupancy = pos.occupancy[BOTH];

        const Square from = pos.king_square(us);

        add_piece_moves(king_attacks(from), from, ownPieces, enemies, occupancy, list);
    }

    /**
     * @brief Attempts to add one castling move.
     *
     * The transit square is rhe square the king crosses between its starting and destination squares.
     */
    inline void try_add_castle(const Position& pos, MoveList& list, Color them, uint8_t right, Square from, Square to,
                               Bitboard emptySquares, Square transit, MoveFlag type)
    {
        if (!(pos.castlingRights & right))
            return;

        if (pos.occupancy[BOTH] & emptySquares)
            return;

        if (pos.is_square_attacked(from, them) || pos.is_square_attacked(transit, them) || pos.is_square_attacked(to, them))
        {
            return;
        }

        list.add(Move(from, to, type));
    }

    void add_castling_moves(const Position& pos, MoveList& list)
    {
        const Color us = pos.sideToMove;
        const Color them = static_cast<Color>(us ^ 1);
        const Square kingSquare = pos.king_square(us);

        if (us == WHITE)
        {
            // O-O: f1 and g1 must be empty; king crosses f1.
            try_add_castle(pos, list, them, WHITE_OO, kingSquare, G1, square_bb(F1) | square_bb(G1), F1, KING_CASTLE);

            // O-O-O: b1, c1 and d1 must be empty; king crosses d1.
            try_add_castle(pos, list, them, WHITE_OOO, kingSquare, C1, square_bb(B1) | square_bb(C1) | square_bb(D1), D1,
                           QUEEN_CASTLE);
        }
        else
        {
            // O-O: f8 and g8 must be empty; king crosses f8.
            try_add_castle(pos, list, them, BLACK_OO, kingSquare, G8, square_bb(F8) | square_bb(G8), F8, KING_CASTLE);

            // O-O-O: b8, c8 and d8 must be empty; king crosses d8.
            try_add_castle(pos, list, them, BLACK_OOO, kingSquare, C8, square_bb(B8) | square_bb(C8) | square_bb(D8), D8,
                           QUEEN_CASTLE);
        }
    }
} // namespace

namespace MoveGen
{
    /**
     * @brief Generates all pseudo-legal moves for the side to move.
     *
     * King-safety filtering is performed separately.
     */
    void generate_pseudo_legal_moves(const Position& pos, MoveList& list)
    {
        list.count = 0;

        add_pawn_moves(pos, list);
        add_knight_moves(pos, list);
        add_slider_moves(pos, list);
        add_king_moves(pos, list);
        add_castling_moves(pos, list);
    }

    void generate_legal_moves(Position& pos, MoveList& legalMoves)
    {
        MoveList pseudoLegalMoves;
        generate_pseudo_legal_moves(pos, pseudoLegalMoves);

        legalMoves.count = 0;
        const Color us = pos.sideToMove;

        for (int i = 0; i < pseudoLegalMoves.count; ++i)
        {
            const Move move = pseudoLegalMoves.moves[i];
            pos.make_move(move);
            const Square kingSquare = pos.king_square(us);
            const bool legal = !pos.is_square_attacked(kingSquare, static_cast<Color>(us ^ 1));

            pos.unmake_move(move);

            if (legal)
                legalMoves.add(move);
        }
    }
} // namespace MoveGen