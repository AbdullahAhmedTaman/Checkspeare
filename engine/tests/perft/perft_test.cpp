#include "../../include/attacks.h"
#include "../../include/position.h"
#include "../../include/zobrist.h"
#include "perft.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Perft Leaf Node Verification", "[perft][movegen]")
{
    // Ensure Zobrist key arrays are initialized before testing positions
    Zobrist::init();
    init_attacks();

    SECTION("Initial Board Position")
    {
        Position pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        REQUIRE(Perft::run(pos, 1) == 20ULL);
        REQUIRE(Perft::run(pos, 2) == 400ULL);
        REQUIRE(Perft::run(pos, 3) == 8902ULL);
        REQUIRE(Perft::run(pos, 4) == 197281ULL);
        REQUIRE(Perft::run(pos, 5) == 4865609ULL);
    }

    SECTION("KiwiPete (Position 2 - Complex Pins, Checks & Castling)")
    {
        Position pos = Position::from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

        REQUIRE(Perft::run(pos, 1) == 48ULL);
        REQUIRE(Perft::run(pos, 2) == 2039ULL);
        REQUIRE(Perft::run(pos, 3) == 97862ULL);
        REQUIRE(Perft::run(pos, 4) == 4085603ULL);
    }

    SECTION("Position 3 (En Passant & Castling Edge Cases)")
    {
        Position pos = Position::from_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

        REQUIRE(Perft::run(pos, 1) == 14ULL);
        REQUIRE(Perft::run(pos, 2) == 191ULL);
        REQUIRE(Perft::run(pos, 3) == 2812ULL);
        REQUIRE(Perft::run(pos, 4) == 43238ULL);
        REQUIRE(Perft::run(pos, 5) == 674624ULL);
    }

    SECTION("Position 4 (Promotions & Discovered Checks)")
    {
        Position pos = Position::from_fen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");

        REQUIRE(Perft::run(pos, 1) == 6ULL);
        REQUIRE(Perft::run(pos, 2) == 264ULL);
        REQUIRE(Perft::run(pos, 3) == 9467ULL);
        REQUIRE(Perft::run(pos, 4) == 422333ULL);
    }
}

TEST_CASE("Zobrist State Integrity Check", "[perft][zobrist]")
{
    Zobrist::init();
    Position pos = Position::from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    Key initial_key = pos.key;

    // Run perft at depth 3 to stress-test thousands of make_move / unmake_move cycles
    Perft::run(pos, 3);

    // Verify Zobrist key is perfectly restored after full tree traversal
    REQUIRE(pos.key == initial_key);
}