// test_attacks.cpp
//
// Thorough Catch2 test-suite for the piece-attack generators in attacks.cpp
// (pawn_attacks, knight_attacks, king_attacks, bishop_attacks, rook_attacks,
// queen_attacks, init_attacks).
//
// ---------------------------------------------------------------------------
// Assumptions made about the rest of the codebase (adjust names if yours differ):
//
//   - types.h declares something like
//         enum Square { A1, B1, ..., H8, NONE };
//         enum Color  { WHITE, BLACK };
//     i.e. standard "file-major within rank" naming where <FILE><RANK>
//     really is that chess square. The test logic below never depends on the
//     *numeric* value of a Square, only on these names meaning the right
//     real-world square, so it's robust to whatever bit-numbering scheme you
//     actually use internally.
//
//   - bitboard.h declares `square_bb`, `reverse_bb`, `file_bb`, `rank_bb`,
//     `diagonal_bb`, `anti_diagonal_bb`, the eight `shift_*(Bitboard)`
//     helpers used by attacks.cpp, and `all_squares()` (a range over the 64
//     real squares, i.e. it must NOT include NONE - attacks.cpp itself
//     relies on that to safely index arrays sized `[Square::NONE]`).
//
//   - IMPORTANT PRECONDITION: bishop_attacks/rook_attacks/queen_attacks use
//     the classic "o - 2s" (Hyperbola Quintessence) trick in ray_attacks().
//     That trick requires the `occ` bitboard passed in to already include
//     the moving piece's own square bit - exactly like a real board's total
//     occupancy bitboard would, since the slider itself occupies its square.
//     Every test below builds `occ` accordingly (see `occ_with`). Passing an
//     `occ` that omits the slider's own bit is a misuse of the API, not
//     something these tests attempt to characterize.
//
// Build notes:
//   - Uses Catch2 v3 (`catch2/catch_all.hpp`). Swap for the more granular
//     `catch2/catch_test_macros.hpp` if you want faster compiles once things
//     are green.
//   - Make sure both your `include/` and the folder containing bitboard.h /
//     types.h are on this target's include path.
//   - This file does not define main(); link against Catch2::Catch2WithMain
//     (or your existing test runner) as you already do for other test files.
// ---------------------------------------------------------------------------

#include <catch2/catch_all.hpp>

#include "board/attacks.h"
#include "board/bitboard.h"
#include "board/types.h"

#include <cstdint>
#include <initializer_list>
#include <random>
#include <vector>

namespace
{
    // ---- small helpers ------------------------------------------------

    Bitboard bb_of(std::initializer_list<Square> squares)
    {
        Bitboard result = 0;
        for (Square sq : squares)
            result |= square_bb(sq);
        return result;
    }

    bool contains(Bitboard bb, Square sq)
    {
        return (bb & square_bb(sq)) != 0;
    }

    // Occupancy for sliding-piece tests must include the slider's own square
    // (see precondition note above). `extra` are any additional occupied
    // squares (blockers).
    Bitboard occ_with(Square slider, std::initializer_list<Square> extra = {})
    {
        Bitboard result = square_bb(slider);
        for (Square sq : extra)
            result |= square_bb(sq);
        return result;
    }

    Bitboard random_bb(std::mt19937_64& rng)
    {
        return static_cast<Bitboard>(rng());
    }

    // Independent (naive) ray-casting used to differentially test the sliding
    // attack generators. Walks one step at a time using the already-available
    // shift primitives and stops at (and includes) the first occupied square -
    // ordinary chess semantics, implemented completely independently of the
    // Hyperbola-Quintessence trick used in attacks.cpp.
    Bitboard naive_ray(Bitboard occ, Bitboard (*step)(Bitboard), Square sq)
    {
        Bitboard attacks = 0;
        Bitboard cur = square_bb(sq);
        for (;;)
        {
            cur = step(cur);
            if (cur == 0)
                break;
            attacks |= cur;
            if (cur & occ)
                break;
        }
        return attacks;
    }

    Bitboard naive_bishop_attacks(Bitboard occ, Square sq)
    {
        return naive_ray(occ, shift_north_east, sq) | naive_ray(occ, shift_north_west, sq) |
               naive_ray(occ, shift_south_east, sq) | naive_ray(occ, shift_south_west, sq);
    }

    Bitboard naive_rook_attacks(Bitboard occ, Square sq)
    {
        return naive_ray(occ, shift_north, sq) | naive_ray(occ, shift_south, sq) | naive_ray(occ, shift_east, sq) |
               naive_ray(occ, shift_west, sq);
    }

    Bitboard naive_queen_attacks(Bitboard occ, Square sq)
    {
        return naive_bishop_attacks(occ, sq) | naive_rook_attacks(occ, sq);
    }

    std::vector<Square> every_square()
    {
        std::vector<Square> squares;
        for (Square sq : all_squares())
            squares.push_back(sq);
        return squares;
    }
} // namespace

// =====================================================================
// knight_attacks
// =====================================================================

TEST_CASE("knight_attacks: exact attack sets on representative squares", "[attacks][knight]")
{
    init_attacks();

    // Corners: exactly 2 moves each.
    CHECK(knight_attacks(Square::A1) == bb_of({Square::B3, Square::C2}));
    CHECK(knight_attacks(Square::H1) == bb_of({Square::F2, Square::G3}));
    CHECK(knight_attacks(Square::A8) == bb_of({Square::B6, Square::C7}));
    CHECK(knight_attacks(Square::H8) == bb_of({Square::F7, Square::G6}));

    // Adjacent-to-corner: exactly 3 moves.
    CHECK(knight_attacks(Square::B1) == bb_of({Square::A3, Square::C3, Square::D2}));

    // Plain edge squares (not corner-adjacent): exactly 4 moves.
    CHECK(knight_attacks(Square::A4) == bb_of({Square::B2, Square::B6, Square::C3, Square::C5}));
    CHECK(knight_attacks(Square::D1) == bb_of({Square::B2, Square::C3, Square::E3, Square::F2}));

    // One square in from the edge: exactly 6 moves.
    CHECK(knight_attacks(Square::B4) == bb_of({Square::A2, Square::A6, Square::C2, Square::C6, Square::D3, Square::D5}));

    // Fully interior square: exactly 8 moves.
    CHECK(knight_attacks(Square::D4) ==
          bb_of({Square::B3, Square::B5, Square::C2, Square::C6, Square::E2, Square::E6, Square::F3, Square::F5}));
}

TEST_CASE("knight_attacks: invariants across the whole board", "[attacks][knight]")
{
    init_attacks();

    int total = 0;
    for (Square sq : every_square())
    {
        Bitboard attacks = knight_attacks(sq);

        INFO("square index = " << static_cast<int>(sq));
        CHECK_FALSE(contains(attacks, sq)); // never attacks its own square

        int count = popcount(attacks);
        CHECK((count == 2 || count == 3 || count == 4 || count == 6 || count == 8));
        total += count;
    }

    // Well-known invariant: a knight has exactly 336 pseudo-legal moves summed
    // over all 64 squares of an empty 8x8 board.
    CHECK(total == 336);
}

TEST_CASE("knight_attacks: attacks are reciprocal", "[attacks][knight]")
{
    init_attacks();

    for (Square from : every_square())
    {
        Bitboard attacks = knight_attacks(from);
        for (Square to : every_square())
        {
            if (contains(attacks, to))
            {
                INFO("from=" << static_cast<int>(from) << " to=" << static_cast<int>(to));
                CHECK(contains(knight_attacks(to), from));
            }
        }
    }
}

// =====================================================================
// king_attacks
// =====================================================================

TEST_CASE("king_attacks: exact attack sets on representative squares", "[attacks][king]")
{
    init_attacks();

    // Corners: exactly 3 neighbours.
    CHECK(king_attacks(Square::A1) == bb_of({Square::A2, Square::B1, Square::B2}));
    CHECK(king_attacks(Square::H1) == bb_of({Square::G1, Square::G2, Square::H2}));
    CHECK(king_attacks(Square::A8) == bb_of({Square::A7, Square::B7, Square::B8}));
    CHECK(king_attacks(Square::H8) == bb_of({Square::G7, Square::G8, Square::H7}));

    // Edge, non-corner: exactly 5 neighbours.
    CHECK(king_attacks(Square::D1) == bb_of({Square::C1, Square::E1, Square::C2, Square::D2, Square::E2}));
    CHECK(king_attacks(Square::A4) == bb_of({Square::A3, Square::A5, Square::B3, Square::B4, Square::B5}));

    // Interior: exactly 8 neighbours.
    CHECK(king_attacks(Square::D4) ==
          bb_of({Square::C3, Square::C4, Square::C5, Square::D3, Square::D5, Square::E3, Square::E4, Square::E5}));
}

TEST_CASE("king_attacks: invariants across the whole board", "[attacks][king]")
{
    init_attacks();

    int total = 0;
    for (Square sq : every_square())
    {
        Bitboard attacks = king_attacks(sq);

        INFO("square index = " << static_cast<int>(sq));
        CHECK_FALSE(contains(attacks, sq));

        int count = popcount(attacks);
        CHECK((count == 3 || count == 5 || count == 8));
        total += count;
    }

    // 4 corners * 3 + 24 edge squares * 5 + 36 interior squares * 8 = 420.
    CHECK(total == 420);
}

TEST_CASE("king_attacks: attacks are reciprocal", "[attacks][king]")
{
    init_attacks();

    for (Square from : every_square())
    {
        Bitboard attacks = king_attacks(from);
        for (Square to : every_square())
        {
            if (contains(attacks, to))
            {
                INFO("from=" << static_cast<int>(from) << " to=" << static_cast<int>(to));
                CHECK(contains(king_attacks(to), from));
            }
        }
    }
}

// =====================================================================
// init_attacks
// =====================================================================

TEST_CASE("init_attacks: idempotent across repeated calls", "[attacks][init]")
{
    init_attacks();

    std::vector<Bitboard> knight_before;
    std::vector<Bitboard> king_before;
    for (Square sq : every_square())
    {
        knight_before.push_back(knight_attacks(sq));
        king_before.push_back(king_attacks(sq));
    }

    init_attacks();
    init_attacks();

    std::size_t i = 0;
    for (Square sq : every_square())
    {
        CHECK(knight_attacks(sq) == knight_before[i]);
        CHECK(king_attacks(sq) == king_before[i]);
        ++i;
    }
}

// =====================================================================
// pawn_attacks
// =====================================================================

TEST_CASE("pawn_attacks: white pawns on central squares", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::WHITE, Square::D4) == bb_of({Square::C5, Square::E5}));
    CHECK(pawn_attacks(Color::WHITE, Square::E2) == bb_of({Square::D3, Square::F3}));
}

TEST_CASE("pawn_attacks: white pawns on edge files only attack one square", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::WHITE, Square::A4) == bb_of({Square::B5}));
    CHECK(pawn_attacks(Color::WHITE, Square::H4) == bb_of({Square::G5}));
    CHECK(pawn_attacks(Color::WHITE, Square::A1) == bb_of({Square::B2}));
}

TEST_CASE("pawn_attacks: white pawns cannot attack past the 8th rank", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::WHITE, Square::D8) == 0);
    CHECK(pawn_attacks(Color::WHITE, Square::A8) == 0);
    CHECK(pawn_attacks(Color::WHITE, Square::H8) == 0);
}

TEST_CASE("pawn_attacks: black pawns on central squares", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::BLACK, Square::D4) == bb_of({Square::C3, Square::E3}));
    CHECK(pawn_attacks(Color::BLACK, Square::E7) == bb_of({Square::D6, Square::F6}));
}

TEST_CASE("pawn_attacks: black pawns on edge files only attack one square", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::BLACK, Square::A4) == bb_of({Square::B3}));
    CHECK(pawn_attacks(Color::BLACK, Square::H4) == bb_of({Square::G3}));
    CHECK(pawn_attacks(Color::BLACK, Square::H8) == bb_of({Square::G7}));
}

TEST_CASE("pawn_attacks: black pawns cannot attack past the 1st rank", "[attacks][pawn]")
{
    CHECK(pawn_attacks(Color::BLACK, Square::D1) == 0);
    CHECK(pawn_attacks(Color::BLACK, Square::A1) == 0);
    CHECK(pawn_attacks(Color::BLACK, Square::H1) == 0);
}

TEST_CASE("pawn_attacks: attack count is always 0, 1 or 2", "[attacks][pawn]")
{
    for (Square sq : every_square())
    {
        CHECK(popcount(pawn_attacks(Color::WHITE, sq)) <= 2);
        CHECK(popcount(pawn_attacks(Color::BLACK, sq)) <= 2);
    }
}

// =====================================================================
// bishop_attacks
// =====================================================================

TEST_CASE("bishop_attacks: empty board, central square", "[attacks][bishop]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4);
    Bitboard expected = bb_of({
        Square::A1,
        Square::B2,
        Square::C3,
        Square::E5,
        Square::F6,
        Square::G7,
        Square::H8,
        Square::A7,
        Square::B6,
        Square::C5,
        Square::E3,
        Square::F2,
        Square::G1,
    });

    CHECK(bishop_attacks(occ, Square::D4) == expected);
    CHECK(popcount(expected) == 13);
}

TEST_CASE("bishop_attacks: empty board, corner square only has one diagonal", "[attacks][bishop]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::A1);
    Bitboard expected = bb_of({Square::B2, Square::C3, Square::D4, Square::E5, Square::F6, Square::G7, Square::H8});

    CHECK(bishop_attacks(occ, Square::A1) == expected);
    CHECK(popcount(expected) == 7);
}

TEST_CASE("bishop_attacks: empty board, edge square", "[attacks][bishop]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::A4);
    Bitboard expected = bb_of({Square::B5, Square::C6, Square::D7, Square::E8, Square::B3, Square::C2, Square::D1});

    CHECK(bishop_attacks(occ, Square::A4) == expected);
}

TEST_CASE("bishop_attacks: a single blocker stops the ray and is itself attacked", "[attacks][bishop]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4, {Square::C3});
    Bitboard attacks = bishop_attacks(occ, Square::D4);

    CHECK(contains(attacks, Square::C3));       // the blocker can be captured
    CHECK_FALSE(contains(attacks, Square::B2)); // nothing beyond it
    CHECK_FALSE(contains(attacks, Square::A1));

    // unobstructed directions are unaffected
    CHECK(contains(attacks, Square::E5));
    CHECK(contains(attacks, Square::H8));
    CHECK(contains(attacks, Square::A7));
    CHECK(contains(attacks, Square::G1));
}

TEST_CASE("bishop_attacks: blockers on every diagonal ray produce a diamond of attacks", "[attacks][bishop]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4, {Square::C3, Square::E5, Square::C5, Square::E3});
    Bitboard expected = bb_of({Square::C3, Square::E5, Square::C5, Square::E3});

    CHECK(bishop_attacks(occ, Square::D4) == expected);
}

TEST_CASE("bishop_attacks: never attacks its own square, and never leaves its two diagonals", "[attacks][bishop]")
{
    init_attacks();
    std::mt19937_64 rng(20240521ULL);

    for (Square sq : every_square())
    {
        for (int trial = 0; trial < 20; ++trial)
        {
            Bitboard occ = random_bb(rng) | square_bb(sq);
            Bitboard attacks = bishop_attacks(occ, sq);
            Bitboard both_diagonals = diagonal_bb(sq) | anti_diagonal_bb(sq);

            INFO("square=" << static_cast<int>(sq) << " occ=0x" << std::hex << static_cast<std::uint64_t>(occ));
            CHECK_FALSE(contains(attacks, sq));
            CHECK((attacks & ~both_diagonals) == 0);
        }
    }
}

TEST_CASE("bishop_attacks: matches an independent ray-casting reference implementation", "[attacks][bishop][fuzz]")
{
    init_attacks();
    std::mt19937_64 rng(987654321ULL);

    for (Square sq : every_square())
    {
        for (int trial = 0; trial < 50; ++trial)
        {
            Bitboard occ = random_bb(rng) | square_bb(sq);

            INFO("square=" << static_cast<int>(sq) << " occ=0x" << std::hex << static_cast<std::uint64_t>(occ));
            CHECK(bishop_attacks(occ, sq) == naive_bishop_attacks(occ, sq));
        }
    }
}

TEST_CASE("bishop_attacks: aggregate mobility on an empty board is 560", "[attacks][bishop]")
{
    init_attacks();
    int total = 0;
    for (Square sq : every_square())
        total += popcount(bishop_attacks(occ_with(sq), sq));
    CHECK(total == 560);
}

// =====================================================================
// rook_attacks
// =====================================================================

TEST_CASE("rook_attacks: empty board, central square", "[attacks][rook]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4);
    Bitboard expected = bb_of({
        Square::A4,
        Square::B4,
        Square::C4,
        Square::E4,
        Square::F4,
        Square::G4,
        Square::H4,
        Square::D1,
        Square::D2,
        Square::D3,
        Square::D5,
        Square::D6,
        Square::D7,
        Square::D8,
    });

    CHECK(rook_attacks(occ, Square::D4) == expected);
    CHECK(popcount(expected) == 14);
}

TEST_CASE("rook_attacks: empty board, corner square", "[attacks][rook]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::A1);
    Bitboard expected = bb_of({
        Square::B1,
        Square::C1,
        Square::D1,
        Square::E1,
        Square::F1,
        Square::G1,
        Square::H1,
        Square::A2,
        Square::A3,
        Square::A4,
        Square::A5,
        Square::A6,
        Square::A7,
        Square::A8,
    });

    CHECK(rook_attacks(occ, Square::A1) == expected);
}

TEST_CASE("rook_attacks: a rook always sees exactly 14 squares on an empty board", "[attacks][rook]")
{
    init_attacks();
    for (Square sq : every_square())
        CHECK(popcount(rook_attacks(occ_with(sq), sq)) == 14);
}

TEST_CASE("rook_attacks: blockers stop the ray in each direction independently", "[attacks][rook]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4, {Square::D6, Square::B4});
    Bitboard attacks = rook_attacks(occ, Square::D4);

    Bitboard expected = bb_of({
        Square::D1,
        Square::D2,
        Square::D3, // south, unobstructed
        Square::D5,
        Square::D6, // north, stops at D6
        Square::C4,
        Square::B4, // west, stops at B4
        Square::E4,
        Square::F4,
        Square::G4,
        Square::H4, // east, unobstructed
    });

    CHECK(attacks == expected);
    CHECK_FALSE(contains(attacks, Square::D7));
    CHECK_FALSE(contains(attacks, Square::D8));
    CHECK_FALSE(contains(attacks, Square::A4));
}

TEST_CASE("rook_attacks: a rook boxed in on all four sides only attacks its neighbours", "[attacks][rook]")
{
    init_attacks();
    Bitboard occ = occ_with(Square::D4, {Square::D5, Square::D3, Square::C4, Square::E4});
    Bitboard expected = bb_of({Square::D5, Square::D3, Square::C4, Square::E4});

    CHECK(rook_attacks(occ, Square::D4) == expected);
}

TEST_CASE("rook_attacks: never attacks its own square, and never leaves its rank/file", "[attacks][rook]")
{
    init_attacks();
    std::mt19937_64 rng(1122334455ULL);

    for (Square sq : every_square())
    {
        for (int trial = 0; trial < 20; ++trial)
        {
            Bitboard occ = random_bb(rng) | square_bb(sq);
            Bitboard attacks = rook_attacks(occ, sq);
            Bitboard rank_and_file = file_bb(sq) | rank_bb(sq);

            INFO("square=" << static_cast<int>(sq) << " occ=0x" << std::hex << static_cast<std::uint64_t>(occ));
            CHECK_FALSE(contains(attacks, sq));
            CHECK((attacks & ~rank_and_file) == 0);
        }
    }
}

TEST_CASE("rook_attacks: matches an independent ray-casting reference implementation", "[attacks][rook][fuzz]")
{
    init_attacks();
    std::mt19937_64 rng(13572468ULL);

    for (Square sq : every_square())
    {
        for (int trial = 0; trial < 50; ++trial)
        {
            Bitboard occ = random_bb(rng) | square_bb(sq);

            INFO("square=" << static_cast<int>(sq) << " occ=0x" << std::hex << static_cast<std::uint64_t>(occ));
            CHECK(rook_attacks(occ, sq) == naive_rook_attacks(occ, sq));
        }
    }
}

TEST_CASE("rook_attacks: aggregate mobility on an empty board is 896", "[attacks][rook]")
{
    init_attacks();
    int total = 0;
    for (Square sq : every_square())
        total += popcount(rook_attacks(occ_with(sq), sq));
    CHECK(total == 896);
}

// =====================================================================
// queen_attacks
// =====================================================================

TEST_CASE("queen_attacks: equals the union of bishop and rook attacks (empty board)", "[attacks][queen]")
{
    init_attacks();
    for (Square sq : every_square())
    {
        Bitboard occ = occ_with(sq);
        CHECK(queen_attacks(occ, sq) == (bishop_attacks(occ, sq) | rook_attacks(occ, sq)));
    }
}

TEST_CASE("queen_attacks: equals the union of bishop and rook attacks (random occupancy)", "[attacks][queen][fuzz]")
{
    init_attacks();
    std::mt19937_64 rng(24681357ULL);

    for (Square sq : every_square())
    {
        for (int trial = 0; trial < 30; ++trial)
        {
            Bitboard occ = random_bb(rng) | square_bb(sq);

            INFO("square=" << static_cast<int>(sq) << " occ=0x" << std::hex << static_cast<std::uint64_t>(occ));
            CHECK(queen_attacks(occ, sq) == (bishop_attacks(occ, sq) | rook_attacks(occ, sq)));
            CHECK(queen_attacks(occ, sq) == naive_queen_attacks(occ, sq));
        }
    }
}

TEST_CASE("queen_attacks: mobility on an empty board matches known central/corner values", "[attacks][queen]")
{
    init_attacks();
    CHECK(popcount(queen_attacks(occ_with(Square::D4), Square::D4)) == 27);
    CHECK(popcount(queen_attacks(occ_with(Square::A1), Square::A1)) == 21);
}

TEST_CASE("queen_attacks: aggregate mobility on an empty board is 1456", "[attacks][queen]")
{
    init_attacks();
    int total = 0;
    for (Square sq : every_square())
        total += popcount(queen_attacks(occ_with(sq), sq));
    CHECK(total == 1456);
}
