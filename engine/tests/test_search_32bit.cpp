#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"
#include "search/move_ordering.h"
#include "search/transposition.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("32-bit Move: Memory layout and alignment constraints", "[move][architecture]")
{
    // The engine relies on these exact sizes to prevent cache misses
    REQUIRE(sizeof(Move) == 4);
    REQUIRE((alignof(Move) == 2 || alignof(Move) == 4));
}

TEST_CASE("32-bit Move: Equality operator strictly ignores ordering scores", "[move][architecture]")
{
    Move m1(E2, E4, DOUBLE_PAWN_PUSH);
    Move m2(E2, E4, DOUBLE_PAWN_PUSH);

    // Mutate the score of m1 to simulate a MovePicker evaluation
    m1.set_score(MoveOrdering::KILLER_SCORE_1);
    m2.set_score(0);

    // Identity must remain intact for TT hits and killer matching
    REQUIRE(m1 == m2);
    REQUIRE_FALSE(m1 != m2);
    REQUIRE(m1.raw() == m2.raw());
    REQUIRE(m1.ordering_score() != m2.ordering_score());
}

TEST_CASE("MovePicker: Zero-allocation sorting correctly mutates scores in-place", "[ordering][movepicker]")
{
    Position pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    // Inject a dummy TT move to verify highest priority extraction
    Move tt_move(E2, E4, DOUBLE_PAWN_PUSH);
    MoveOrdering::MovePicker picker(list, pos, tt_move, 0);

    Move first_pick = picker.next_move();

    // 1. TT move must be returned first
    REQUIRE(first_pick == tt_move);
    // 2. The score must have been actively mutated by score_moves()
    REQUIRE(first_pick.ordering_score() == MoveOrdering::TT_MOVE_SCORE);

    // Ensure the list drains correctly without bounds violations
    int remaining_count = picker.remaining();
    while (picker.remaining() > 0)
    {
        Move m = picker.next_move();
        REQUIRE(m.is_ok());
        remaining_count--;
    }
    REQUIRE(remaining_count == 0);
    REQUIRE_FALSE(picker.next_move().is_ok());
}

TEST_CASE("TranspositionTable: Cache line density and strict 16-byte packing", "[tt][architecture]")
{
    // A single byte of overflow here halves engine NPS. These must pass.
    REQUIRE(sizeof(Search::TTEntry) == 16);
    REQUIRE(sizeof(Search::TTBucket) == 64);

    Search::TT.resize(1); // 1 MB
    Search::TT.clear();

    const Key mock_key = 0xDEADBEEFCAFEBABEULL;
    Move mock_move(A7, A8, QUEEN_PROMO);
    mock_move.set_score(MoveOrdering::PROMOTION_QUEEN); // Transient score

    Search::TT.store(mock_key, 0, 10, 50, Search::TT_EXACT, mock_move);

    Search::TTEntry entry;
    bool hit = Search::TT.probe(mock_key, 0, entry);

    REQUIRE(hit);
    REQUIRE(entry.key == mock_key);
    REQUIRE(entry.depth == 10);
    REQUIRE(entry.score == 50);

    // The TT must store the physical move, but wipe the transient score to 0
    // to prevent pollution across different search depths.
    REQUIRE(entry.best_move == mock_move);
    REQUIRE(entry.best_move.ordering_score() == 0);
}