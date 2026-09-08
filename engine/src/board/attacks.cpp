#include "../include/attacks.h"
#include "bitboard.h"
#include "types.h"

namespace
{
    Bitboard arrKnightAttacks[Square::SQ_NONE];
    Bitboard arrKingAttacks[Square::SQ_NONE];

    Bitboard calculate_knight_attacks(Square sq)
    {
        Bitboard bb = square_bb(sq);
        Bitboard attacks = 0;

        attacks |= shift_north(shift_north(shift_east(bb)));
        attacks |= shift_north(shift_north(shift_west(bb)));

        attacks |= shift_south(shift_south(shift_east(bb)));
        attacks |= shift_south(shift_south(shift_west(bb)));

        attacks |= shift_east(shift_east(shift_north(bb)));
        attacks |= shift_east(shift_east(shift_south(bb)));

        attacks |= shift_west(shift_west(shift_north(bb)));
        attacks |= shift_west(shift_west(shift_south(bb)));

        return attacks;
    }

    Bitboard calculate_king_attacks(Square sq)
    {
        Bitboard bb = square_bb(sq);

        return shift_north(bb) | shift_south(bb) | shift_east(bb) | shift_west(bb) | shift_north_east(bb) |
               shift_south_east(bb) | shift_north_west(bb) | shift_south_west(bb);
    }
} // namespace

/**
 * @brief Have to be called once at the start of the engine to init attacks.
 *
 */
void init_attacks()
{
    for (Square sq : all_squares())
    {
        arrKnightAttacks[sq] = calculate_knight_attacks(sq);
        arrKingAttacks[sq] = calculate_king_attacks(sq);
    }
}

Bitboard pawn_attacks(Color color, Square sq)
{
    Bitboard bb = square_bb(sq);

    if (color == Color::WHITE)
        return shift_north_east(bb) | shift_north_west(bb);

    return shift_south_east(bb) | shift_south_west(bb);
}

Bitboard knight_attacks(Square sq)
{
    return arrKnightAttacks[sq];
}

Bitboard king_attacks(Square sq)
{
    return arrKingAttacks[sq];
}
