// tests/test_search.cpp
// Catch2 v3 test suite for the UCI chess engine's search subsystem.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "../include/attacks.h"
#include "../include/bitboard.h"
#include "../include/evaluate.h"
#include "../include/heuristics.h"
#include "../include/move_ordering.h"
#include "../include/movegen.h"
#include "../include/position.h"
#include "../include/search.h"
#include "../include/transposition.h"
#include "../include/types.h"
#include "../include/zobrist.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// -----------------------------------------------------------------------------
// Forward declarations for symbols not exposed in public headers.
// -----------------------------------------------------------------------------
namespace Search
{
    void init_lmr_table();
    extern int LMRTable[MAX_PLY][MAX_MOVES];
} // namespace Search

bool is_repetition(const Position& pos);

// -----------------------------------------------------------------------------
// One-time engine init (runs before any TEST_CASE).
// -----------------------------------------------------------------------------
namespace
{
    struct EngineTestInit
    {
        EngineTestInit()
        {
            Zobrist::init();
            init_attacks();
            Search::init_lmr_table();
        }
    };

    const EngineTestInit g_engine_test_init;

    Search::SearchContext make_context()
    {
        static SearchLimits limits{};
        return Search::SearchContext(limits, Search::Signals_stop);
    }

    void reset_search_state()
    {
        Search::init();
        std::memset(Search::killer_moves, 0, sizeof(Search::killer_moves));
        std::memset(Search::history_table, 0, sizeof(Search::history_table));
    }

    namespace Fens
    {
        constexpr const char* StartPos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        constexpr const char* AfterE4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        constexpr const char* BlackCheckmated = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
        constexpr const char* BlackStalemate = "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1";
        constexpr const char* WhiteMateIn1 = "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1";

        // Multi-ply forced mates for structural search validation
        constexpr const char* WhiteMateIn2 = "7k/6pp/8/8/8/8/1q6/R6K w - - 0 1";  // 1. Ra8+ Qb8 2. Rxb8#
        constexpr const char* WhiteMateIn3 = "7k/6pp/8/8/8/8/1qr5/R6K w - - 0 1"; // 1. Ra8+ Rc8 2. Rxc8+ Qb8 3. Rxb8#

        constexpr const char* ItalianGame = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
        constexpr const char* PxQNxQ = "7k/8/8/3q4/2P2N2/8/8/7K w - - 0 1";
    } // namespace Fens
} // namespace

// =============================================================================
// Suite 1   Move encoding
// =============================================================================
TEST_CASE("Move encodes from/to/flag correctly2", "[move]")
{
    const Move m(E2, E4, DOUBLE_PAWN_PUSH);
    REQUIRE(m.from() == E2);
    REQUIRE(m.to() == E4);
    REQUIRE(m.flag() == DOUBLE_PAWN_PUSH);
    REQUIRE(m.is_ok());
    REQUIRE_FALSE(m.is_null());
    REQUIRE(m.is_double_push());
    REQUIRE_FALSE(m.is_capture());
}

TEST_CASE("Move::none() and Move::null() have distinct semantics", "[move]")
{
    const Move none = Move::none();
    REQUIRE_FALSE(none.is_ok());
    REQUIRE_FALSE(none.is_null());

    const Move null = Move::null();
    REQUIRE(null.is_ok());
    REQUIRE(null.is_null());
}

// =============================================================================
// Suite 2   Position & FEN parsing
// =============================================================================
TEST_CASE("FEN: start position parses correctly", "[position][fen]")
{
    const Position pos = Position::from_fen(Fens::StartPos);
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0x0F);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.ply == 0);
    REQUIRE(popcount(pos.occupancy[BOTH]) == 32);
    REQUIRE(pos.king_square(WHITE) == E1);
}

// =============================================================================
// Suite 3   Zobrist integration with make/unmake
// =============================================================================
TEST_CASE("make_move keeps pos.key in sync with generate_key()", "[zobrist][position]")
{
    Position pos = Position::from_fen(Fens::StartPos);
    MoveList legal;
    MoveGen::generate_legal_moves(pos, legal);
    REQUIRE(legal.count > 0);
    for (int i = 0; i < legal.count; ++i)
    {
        const Move m = legal.moves[i];
        const Key original = pos.key;
        pos.make_move(m);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(pos.key == original);
    }
}

// =============================================================================
// Suite 4   Null move semantics
// =============================================================================
TEST_CASE("Null move flips side to move, preserves board, clears EP, changes key", "[position][null]")
{
    Position pos = Position::from_fen(Fens::AfterE4);
    const Key k0 = pos.key;
    const Bitboard w0 = pos.occupancy[WHITE];

    pos.make_move(Move::null());
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.occupancy[WHITE] == w0);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.key != k0);

    pos.unmake_move(Move::null());
    REQUIRE(pos.sideToMove == BLACK);
    REQUIRE(pos.key == k0);
}

// =============================================================================
// Suite 5   Move ordering / MovePicker
// =============================================================================
TEST_CASE("MovePicker returns the TT move first", "[ordering]")
{
    Position pos = Position::from_fen(Fens::ItalianGame);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Move tt = Move::none();
    for (int i = 0; i < list.count; ++i)
        if (!list.moves[i].is_capture() && !list.moves[i].is_promotion())
        {
            tt = list.moves[i];
            break;
        }

    MoveOrdering::MovePicker picker(list, pos, tt, 0);
    REQUIRE(picker.next_move() == tt);
}

TEST_CASE("MovePicker ranks MVV-LVA captures before quieter captures", "[ordering]")
{
    Position pos = Position::from_fen(Fens::PxQNxQ);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Move pxq = Move::none();
    Move nxq = Move::none();
    for (int i = 0; i < list.count; ++i)
    {
        if (list.moves[i].from() == C4 && list.moves[i].to() == D5)
            pxq = list.moves[i];
        if (list.moves[i].from() == F4 && list.moves[i].to() == D5)
            nxq = list.moves[i];
    }

    MoveOrdering::MovePicker picker(list, pos, Move::none(), 0);
    REQUIRE(picker.next_move() == pxq);
}

// =============================================================================
// Suite 6   Heuristics (killers + history)
// =============================================================================
TEST_CASE("update_killers shifts slots correctly", "[heuristics]")
{
    reset_search_state();
    const Move a(E2, E4, QUIET_MOVE);
    const Move b(G1, F3, QUIET_MOVE);

    Search::update_killers(a, 5);
    REQUIRE(Search::killer_moves[5][0] == a);

    Search::update_killers(b, 5);
    REQUIRE(Search::killer_moves[5][0] == b);
    REQUIRE(Search::killer_moves[5][1] == a);
}

// =============================================================================
// Suite 7   LMR table
// =============================================================================
TEST_CASE("LMRTable is monotonic non-decreasing in depth and move count", "[lmr]")
{
    for (int d = 1; d < MAX_PLY; ++d)
        for (int m = 1; m < MAX_MOVES; ++m)
        {
            REQUIRE(Search::LMRTable[d][m] >= Search::LMRTable[d - 1][m]);
            REQUIRE(Search::LMRTable[d][m] >= Search::LMRTable[d][m - 1]);
        }
}

// =============================================================================
// Suite 8   Quiescence
// =============================================================================
TEST_CASE("Quiescence: mate if in check with no evasions", "[search][qsearch][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::BlackCheckmated);
    auto ctx = make_context();
    const Value v = Search::quiescence(pos, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx);
    REQUIRE(v == -VALUE_MATE);
}

// =============================================================================
// Suite 9   Negamax terminal-node handling
// =============================================================================
TEST_CASE("Negamax recognises checkmate at the root", "[search][negamax][terminal]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::BlackCheckmated);
    auto ctx = make_context();
    const Value v = Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == -VALUE_MATE);
}

// =============================================================================
// Suite 10   Multi-ply Tactics and Mate Puzzles
// =============================================================================
TEST_CASE("Negamax finds Mate in 1 at depth 1", "[search][negamax][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::WhiteMateIn1);
    auto ctx = make_context();
    const Value v = Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_MATE - 1);
    REQUIRE(ctx.info.best_move.to() == A8);
}

TEST_CASE("Negamax finds Mate in 2 (Rook Sacrifice)", "[search][negamax][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::WhiteMateIn2);
    auto ctx = make_context();

    // Depth 3 is required for a Mate in 2 (W, B, W)
    const Value v = Search::negamax<Search::ROOT>(pos, 3, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    REQUIRE(v == VALUE_MATE - 3);
    REQUIRE(ctx.info.best_move.from() == A1);
    REQUIRE(ctx.info.best_move.to() == A8);
}

TEST_CASE("Negamax finds Mate in 3 (Double Blockers)", "[search][negamax][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::WhiteMateIn3);
    auto ctx = make_context();

    // Depth 5 is required for a Mate in 3 (W, B, W, B, W)
    const Value v = Search::negamax<Search::ROOT>(pos, 5, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    REQUIRE(v == VALUE_MATE - 5);
    REQUIRE(ctx.info.best_move.from() == A1);
    REQUIRE(ctx.info.best_move.to() == A8);
}

// =============================================================================
// Suite 11   Advanced Search Pruning
// =============================================================================
TEST_CASE("Alpha-Beta pruning drastically reduces searched nodes", "[search][pruning]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::ItalianGame);
    auto ctx = make_context();

    Search::negamax<Search::ROOT>(pos, 4, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    // A raw Minimax tree at depth 4 exceeds 1M+ nodes. Alpha-Beta should easily crush this.
    REQUIRE(ctx.info.nodes > 0);
    REQUIRE(ctx.info.nodes < 50000);
}

TEST_CASE("Null-Move Pruning (NMP) triggers beta cutoffs in quiet positions", "[search][nmp]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos);
    auto ctx = make_context();

    Value static_eval = Eval::evaluate(pos);
    Value out_score = 0;

    // Force a cutoff state by feeding a beta intentionally lower than the static eval
    bool pruned = Search::try_null_move_pruning(pos, 4, 0, static_eval - 10, ctx, out_score);

    REQUIRE(pruned == true);
    REQUIRE(out_score >= static_eval - 10);
}

// =============================================================================
// Suite 12   Transposition table
// =============================================================================
TEST_CASE("TT deeper entry overwrites shallower one in the same key", "[tt][replacement]")
{
    Search::TT.resize(1);
    Search::TT.clear();
    const Key k = 0xABCDEF0123456789ULL;
    const Move shallow_m(A2, A3, QUIET_MOVE);
    const Move deep_m(A2, A4, DOUBLE_PAWN_PUSH);

    Search::TT.store(k, 0, 2, 10, Search::TT_EXACT, shallow_m);
    Search::TT.store(k, 0, 8, 20, Search::TT_EXACT, deep_m);

    Search::TTEntry e;
    REQUIRE(Search::TT.probe(k, 0, e));
    REQUIRE(e.depth == 8);
    REQUIRE(e.best_move == deep_m);
}

// =============================================================================
// Suite 13   Repetition detection
// =============================================================================
TEST_CASE("is_repetition: knight shuffle returns to the start position", "[repetition]")
{
    Position pos = Position::from_fen(Fens::StartPos);
    auto play = [&](Square from, Square to)
    {
        pos.make_move(Move(from, to, QUIET_MOVE));
    };

    play(G1, F3);
    REQUIRE_FALSE(is_repetition(pos));
    play(G8, F6);
    REQUIRE_FALSE(is_repetition(pos));
    play(F3, G1);
    REQUIRE_FALSE(is_repetition(pos));
    play(F6, G8);
    REQUIRE(is_repetition(pos));
}

// =============================================================================
// Suite 14   start_search (UCI loop) smoke & functional tests
// =============================================================================
TEST_CASE("start_search emits a legal bestmove and does not corrupt the position", "[search][uci]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos);
    const Key key_before = pos.key;
    SearchLimits limits{};
    limits.depth = 2;

    std::ostringstream buffer;
    std::streambuf* old_buf = std::cout.rdbuf(buffer.rdbuf());
    Search::start_search(pos, limits);
    std::cout.rdbuf(old_buf);

    REQUIRE_THAT(buffer.str(), Catch::Matchers::ContainsSubstring("bestmove"));
    REQUIRE(pos.key == key_before);
    REQUIRE(pos.ply == 0);
}

TEST_CASE("start_search emits the correct bestmove for a forced mate", "[search][uci]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::WhiteMateIn2);
    SearchLimits limits{};
    limits.depth = 3; // Mate in 2

    std::ostringstream buffer;
    std::streambuf* old_buf = std::cout.rdbuf(buffer.rdbuf());
    Search::start_search(pos, limits);
    std::cout.rdbuf(old_buf);

    // Engine should find 1. Ra8+
    REQUIRE_THAT(buffer.str(), Catch::Matchers::ContainsSubstring("bestmove a1a8"));
}