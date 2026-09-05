#include <catch2/catch_test_macros.hpp>

#include "../include/bitboard.h"

namespace
{

    // Builds a Square from 0-based file/rank (file 0..7 = a..h, rank 0..7 = 1..8),
    // independent of how the project's Square enum happens to name each value.
    constexpr Square make_square(int file, int rank)
    {
        return static_cast<Square>(rank * 8 + file);
    }

} // namespace

TEST_CASE("rank_of and file_of decode a square correctly", "[bitboard]")
{
    CHECK(rank_of(make_square(0, 0)) == 0); // a1
    CHECK(file_of(make_square(0, 0)) == 0);

    CHECK(rank_of(make_square(4, 3)) == 3); // e4
    CHECK(file_of(make_square(4, 3)) == 4);

    CHECK(rank_of(make_square(7, 7)) == 7); // h8
    CHECK(file_of(make_square(7, 7)) == 7);
}

TEST_CASE("square_bb sets exactly one bit", "[bitboard]")
{
    const Square e4 = make_square(4, 3);
    Bitboard bb = square_bb(e4);

    CHECK(popcount(bb) == 1);
    CHECK(get_bit(bb, e4));
}

TEST_CASE("set_bit / get_bit / clear_bit / toggle_bit round-trip", "[bitboard]")
{
    const Square e4 = make_square(4, 3);
    Bitboard bb = BB_EMPTY;

    REQUIRE_FALSE(get_bit(bb, e4));

    set_bit(bb, e4);
    REQUIRE(get_bit(bb, e4));

    toggle_bit(bb, e4);
    REQUIRE_FALSE(get_bit(bb, e4));

    toggle_bit(bb, e4);
    REQUIRE(get_bit(bb, e4));

    clear_bit(bb, e4);
    REQUIRE_FALSE(get_bit(bb, e4));

    // Setting/clearing one square must not disturb any other square.
    const Square a1 = make_square(0, 0);
    const Square h8 = make_square(7, 7);
    set_bit(bb, a1);
    set_bit(bb, h8);
    clear_bit(bb, a1);
    CHECK_FALSE(get_bit(bb, a1));
    CHECK(get_bit(bb, h8));
}

TEST_CASE("popcount counts set bits", "[bitboard]")
{
    CHECK(popcount(BB_EMPTY) == 0);
    CHECK(popcount(BB_FULL) == 64);

    Bitboard bb = BB_EMPTY;
    set_bit(bb, make_square(0, 0));
    set_bit(bb, make_square(4, 3));
    set_bit(bb, make_square(7, 7));
    CHECK(popcount(bb) == 3);
}

TEST_CASE("lsb finds the lowest-indexed set square", "[bitboard]")
{
    Bitboard bb = BB_EMPTY;
    set_bit(bb, make_square(4, 3)); // e4, square index 28
    set_bit(bb, make_square(7, 7)); // h8, square index 63
    CHECK(lsb(bb) == make_square(4, 3));
}

TEST_CASE("pop_lsb drains a bitboard in ascending square order", "[bitboard]")
{
    const Square a1 = make_square(0, 0);
    const Square e4 = make_square(4, 3);
    const Square h8 = make_square(7, 7);

    Bitboard bb = BB_EMPTY;
    set_bit(bb, h8);
    set_bit(bb, a1);
    set_bit(bb, e4);

    CHECK(pop_lsb(bb) == a1);
    CHECK(pop_lsb(bb) == e4);
    CHECK(pop_lsb(bb) == h8);
    CHECK(bb == BB_EMPTY);
}

TEST_CASE("rank_bb returns all eight squares of the square's rank", "[bitboard]")
{
    Bitboard bb = rank_bb(make_square(4, 3)); // e4 -> rank 4
    CHECK(popcount(bb) == 8);
    for (int file = 0; file < 8; ++file)
        CHECK(get_bit(bb, make_square(file, 3)));
    // Nothing from other ranks should be set.
    CHECK_FALSE(get_bit(bb, make_square(4, 0)));
    CHECK_FALSE(get_bit(bb, make_square(4, 7)));
}

TEST_CASE("file_bb returns all eight squares of the square's file", "[bitboard]")
{
    Bitboard bb = file_bb(make_square(4, 3)); // e4 -> e-file
    CHECK(popcount(bb) == 8);
    for (int rank = 0; rank < 8; ++rank)
        CHECK(get_bit(bb, make_square(4, rank)));
    CHECK_FALSE(get_bit(bb, make_square(0, 3)));
    CHECK_FALSE(get_bit(bb, make_square(7, 3)));
}

TEST_CASE("diagonal_bb covers the a1-h8 direction through the square", "[bitboard]")
{
    // a1 and h8 are both on the main diagonal, so they must share one.
    CHECK(diagonal_bb(make_square(0, 0)) == diagonal_bb(make_square(7, 7)));

    Bitboard bb = diagonal_bb(make_square(4, 3)); // e4
    CHECK(get_bit(bb, make_square(4, 3)));        // e4 itself
    CHECK(get_bit(bb, make_square(3, 2)));        // d3, one step toward a1
    CHECK(get_bit(bb, make_square(5, 4)));        // f5, one step toward h8
}

TEST_CASE("diagonal_bb and anti_diagonal_bb corner cases contain only the corner", "[bitboard]")
{
    // a1 sits alone on its anti-diagonal, and h8 sits alone on its.
    CHECK(anti_diagonal_bb(make_square(0, 0)) == square_bb(make_square(0, 0)));
    CHECK(diagonal_bb(make_square(7, 0)) == square_bb(make_square(7, 0))); // h1 alone on its diagonal
}

TEST_CASE("anti_diagonal_bb covers the a8-h1 direction through the square", "[bitboard]")
{
    // a8 and h1 both lie on the same anti-diagonal.
    CHECK(anti_diagonal_bb(make_square(0, 7)) == anti_diagonal_bb(make_square(7, 0)));

    Bitboard bb = anti_diagonal_bb(make_square(4, 3)); // e4
    CHECK(get_bit(bb, make_square(4, 3)));             // e4 itself
    CHECK(get_bit(bb, make_square(3, 4)));             // d5 is one step up-left
    CHECK(get_bit(bb, make_square(5, 2)));             // f3 is one step down-right
}

TEST_CASE("shift functions move a single bit one step in the expected direction", "[bitboard]")
{
    const Bitboard e4 = square_bb(make_square(4, 3));

    CHECK(shift_north_east(e4) == square_bb(make_square(5, 4))); // f5
    CHECK(shift_north_west(e4) == square_bb(make_square(3, 4))); // d5
    CHECK(shift_south_east(e4) == square_bb(make_square(5, 2))); // f3
    CHECK(shift_south_west(e4) == square_bb(make_square(3, 2))); // d3
    CHECK(shift_east(e4) == square_bb(make_square(5, 3)));       // f4
    CHECK(shift_west(e4) == square_bb(make_square(3, 3)));       // d4
}

TEST_CASE("shift_north moves a bit one rank up", "[bitboard]")
{
    const Bitboard e4 = square_bb(make_square(4, 3));
    CHECK(shift_north(e4) == square_bb(make_square(4, 4))); // e5

    // Off the top of the board vanishes rather than wrapping to rank 1.
    const Bitboard e8 = square_bb(make_square(4, 7));
    CHECK(shift_north(e8) == BB_EMPTY);
}

TEST_CASE("shift_south moves a bit one rank down", "[bitboard]")
{
    const Bitboard e4 = square_bb(make_square(4, 3));
    CHECK(shift_south(e4) == square_bb(make_square(4, 2))); // e3

    // Off the bottom of the board vanishes rather than wrapping to rank 8.
    const Bitboard e1 = square_bb(make_square(4, 0));
    CHECK(shift_south(e1) == BB_EMPTY);
}

TEST_CASE("shifts off the edge of the board vanish instead of wrapping", "[bitboard]")
{
    const Bitboard h4 = square_bb(make_square(7, 3));
    const Bitboard a4 = square_bb(make_square(0, 3));

    CHECK(shift_east(h4) == BB_EMPTY);
    CHECK(shift_north_east(h4) == BB_EMPTY);
    CHECK(shift_south_east(h4) == BB_EMPTY);

    CHECK(shift_west(a4) == BB_EMPTY);
    CHECK(shift_north_west(a4) == BB_EMPTY);
    CHECK(shift_south_west(a4) == BB_EMPTY);
}