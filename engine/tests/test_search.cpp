// tests/test_search.cpp
//
// Catch2 v3 test suite for the UCI chess engine's search subsystem.
//
// Build (CMake):
//   add_executable(engine_tests tests/test_search.cpp)
//   target_link_libraries(engine_tests PRIVATE <engine sources> Catch2::Catch2WithMain)
//   target_compile_features(engine_tests PRIVATE cxx_std_20)
//
// Run all:        ./engine_tests
// Run one suite:  ./engine_tests "[tt]"
// List tests:     ./engine_tests --list-tests
// Skip slow:      ./engine_tests ~[slow]

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
    void init_lmr_table();                   // defined in search.cpp
    extern int LMRTable[MAX_PLY][MAX_MOVES]; // defined in search.cpp
} // namespace Search

bool is_repetition(const Position& pos); // free function in position.cpp

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

    // -------------------------------------------------------------------------
    // A SearchContext needs a persistent SearchLimits reference.
    // -------------------------------------------------------------------------
    Search::SearchContext make_context()
    {
        static SearchLimits limits{};
        return Search::SearchContext(limits, Search::Signals_stop);
    }

    // -------------------------------------------------------------------------
    // Reset global search state between test cases.
    //
    // Search::init() only resets the stop flag, so we also clear the global
    // killer table and history table (both persist across tests).
    // -------------------------------------------------------------------------
    void reset_search_state()
    {
        Search::init();
        std::memset(Search::killer_moves, 0, sizeof(Search::killer_moves));
        std::memset(Search::history_table, 0, sizeof(Search::history_table));
    }

    // -------------------------------------------------------------------------
    // FEN constants.
    // -------------------------------------------------------------------------
    namespace Fens
    {
        constexpr const char* StartPos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        constexpr const char* AfterE4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
        constexpr const char* BlackCheckmated = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
        constexpr const char* BlackStalemate = "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1";
        constexpr const char* WhiteMateIn1 = "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1";
        constexpr const char* ItalianGame = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
        // White pawn c4 and knight f4 both attack the black queen on d5.
        constexpr const char* PxQNxQ = "7k/8/8/3q4/2P2N2/8/8/7K w - - 0 1";
    } // namespace Fens
} // namespace

// =============================================================================
// Suite 1 — Move encoding
// =============================================================================
TEST_CASE("Move encodes from/to/flag correctly", "[move]")
{
    const Move m(E2, E4, DOUBLE_PAWN_PUSH);
    REQUIRE(m.from() == E2);
    REQUIRE(m.to() == E4);
    REQUIRE(m.flag() == DOUBLE_PAWN_PUSH);
    REQUIRE(m.is_ok());
    REQUIRE_FALSE(m.is_null());
    REQUIRE(m.is_double_push());
    REQUIRE_FALSE(m.is_capture());
    REQUIRE_FALSE(m.is_promotion());
    REQUIRE_FALSE(m.is_castle());
}

TEST_CASE("Move::none() and Move::null() have distinct semantics", "[move]")
{
    const Move none = Move::none();
    REQUIRE_FALSE(none.is_ok());
    REQUIRE_FALSE(none.is_null()); // flag() == QUIET_MOVE (0)

    const Move null = Move::null();
    REQUIRE(null.is_ok());   // data != 0
    REQUIRE(null.is_null()); // flag() == NULL_MOVE (7)
    REQUIRE_FALSE(null.is_capture());
    REQUIRE_FALSE(null.is_promotion());
}

TEST_CASE("Promotion flags decode correctly", "[move]")
{
    REQUIRE(promotion_type(Move(E7, E8, QUEEN_PROMO)) == QUEEN);
    REQUIRE(promotion_type(Move(E7, E8, ROOK_PROMO)) == ROOK);
    REQUIRE(promotion_type(Move(E7, E8, BISHOP_PROMO)) == BISHOP);
    REQUIRE(promotion_type(Move(E7, E8, KNIGHT_PROMO)) == KNIGHT);

    const Move qc(E7, F8, QUEEN_PROMO_CAP);
    REQUIRE(qc.is_ok());
    REQUIRE(qc.is_promotion());
    REQUIRE(qc.is_capture());
    REQUIRE(promotion_type(qc) == QUEEN);
}

TEST_CASE("Castling flags are recognised", "[move]")
{
    const Move kc(E1, G1, KING_CASTLE);
    REQUIRE(kc.is_castle());
    REQUIRE(kc.is_kingside_castle());
    REQUIRE_FALSE(kc.is_queenside_castle());

    const Move qc(E1, C1, QUEEN_CASTLE);
    REQUIRE(qc.is_castle());
    REQUIRE_FALSE(qc.is_kingside_castle());
    REQUIRE(qc.is_queenside_castle());
}

// =============================================================================
// Suite 2 — Position & FEN parsing
// =============================================================================
TEST_CASE("FEN: start position parses correctly", "[position][fen]")
{
    const Position pos = Position::from_fen(Fens::StartPos);

    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0x0F); // KQkq → bits 0..3
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.ply == 0);

    REQUIRE(popcount(pos.occupancy[WHITE]) == 16);
    REQUIRE(popcount(pos.occupancy[BLACK]) == 16);
    REQUIRE(popcount(pos.occupancy[BOTH]) == 32);

    REQUIRE(pos.king_square(WHITE) == E1);
    REQUIRE(pos.king_square(BLACK) == E8);
    REQUIRE(pos.key != 0);
}

TEST_CASE("FEN: en passant square and side to move parse correctly", "[position][fen]")
{
    const Position pos = Position::from_fen(Fens::AfterE4);
    REQUIRE(pos.sideToMove == BLACK);
    REQUIRE(pos.enPassantSquare == E3);
    REQUIRE(pos.halfmoveClock == 0);
}

TEST_CASE("FEN: start-position evaluation is symmetric", "[position][fen][eval]")
{
    const Position pos = Position::from_fen(Fens::StartPos);
    REQUIRE(pos.mg_score[WHITE] == pos.mg_score[BLACK]);
    REQUIRE(pos.eg_score[WHITE] == pos.eg_score[BLACK]);
    REQUIRE(Eval::evaluate(pos) == 0);
}

// =============================================================================
// Suite 3 — Zobrist integration with make/unmake
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

TEST_CASE("Make/unmake is byte-stable across all legal moves", "[position][make_unmake]")
{
    Position pos = Position::from_fen(Fens::ItalianGame);

    const Key k0 = pos.key;
    const Color stm0 = pos.sideToMove;
    const uint8_t cr0 = pos.castlingRights;
    const Square ep0 = pos.enPassantSquare;
    const uint16_t hm0 = pos.halfmoveClock;
    const int pl0 = pos.ply;
    const int mg0W = pos.mg_score[WHITE];
    const int mg0B = pos.mg_score[BLACK];
    const int gp0 = pos.game_phase;

    MoveList legal;
    MoveGen::generate_legal_moves(pos, legal);
    REQUIRE(legal.count > 0);

    for (int i = 0; i < legal.count; ++i)
    {
        const Move m = legal.moves[i];
        pos.make_move(m);
        REQUIRE(pos.ply == pl0 + 1);
        pos.unmake_move(m);

        REQUIRE(pos.key == k0);
        REQUIRE(pos.sideToMove == stm0);
        REQUIRE(pos.castlingRights == cr0);
        REQUIRE(pos.enPassantSquare == ep0);
        REQUIRE(pos.halfmoveClock == hm0);
        REQUIRE(pos.ply == pl0);
        REQUIRE(pos.mg_score[WHITE] == mg0W);
        REQUIRE(pos.mg_score[BLACK] == mg0B);
        REQUIRE(pos.game_phase == gp0);
    }
}

// =============================================================================
// Suite 4 — Null move semantics
// =============================================================================
TEST_CASE("Null move flips side to move, preserves board, clears EP, changes key", "[position][null]")
{
    Position pos = Position::from_fen(Fens::AfterE4);

    const Key k0 = pos.key;
    const Bitboard w0 = pos.occupancy[WHITE];
    const Bitboard b0 = pos.occupancy[BLACK];
    const Square ep0 = pos.enPassantSquare;

    REQUIRE(pos.sideToMove == BLACK);
    REQUIRE(ep0 == E3);

    pos.make_move(Move::null());
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.occupancy[WHITE] == w0);
    REQUIRE(pos.occupancy[BLACK] == b0);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.key != k0);

    pos.unmake_move(Move::null());
    REQUIRE(pos.sideToMove == BLACK);
    REQUIRE(pos.enPassantSquare == ep0);
    REQUIRE(pos.key == k0);
    REQUIRE(pos.occupancy[WHITE] == w0);
    REQUIRE(pos.occupancy[BLACK] == b0);
}

TEST_CASE("Two consecutive null moves return to the same Zobrist key", "[position][null]")
{
    Position pos = Position::from_fen(Fens::StartPos);
    const Key k0 = pos.key;

    pos.make_move(Move::null());
    const Key k1 = pos.key;
    REQUIRE(k1 != k0);

    pos.make_move(Move::null());
    const Key k2 = pos.key;
    REQUIRE(k2 == k0); // both flips cancel

    pos.unmake_move(Move::null());
    REQUIRE(pos.key == k1);
    pos.unmake_move(Move::null());
    REQUIRE(pos.key == k0);
    REQUIRE(pos.sideToMove == WHITE);
}

// =============================================================================
// Suite 5 — Move ordering / MovePicker
// =============================================================================
TEST_CASE("MovePicker returns the TT move first", "[ordering]")
{
    Position pos = Position::from_fen(Fens::ItalianGame);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    REQUIRE(list.count > 0);

    Move tt = Move::none();
    for (int i = 0; i < list.count; ++i)
        if (!list.moves[i].is_capture() && !list.moves[i].is_promotion())
        {
            tt = list.moves[i];
            break;
        }
    REQUIRE(tt.is_ok());

    MoveOrdering::MovePicker picker(list, pos, tt, /*ply=*/0);
    REQUIRE(picker.next_move() == tt);
}

TEST_CASE("MovePicker ranks MVV-LVA captures before quieter captures", "[ordering]")
{
    // Pawn c4 and knight f4 both attack the black queen on d5.
    Position pos = Position::from_fen(Fens::PxQNxQ);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Move pxq = Move::none();
    Move nxq = Move::none();
    for (int i = 0; i < list.count; ++i)
    {
        const Move m = list.moves[i];
        if (!m.is_capture())
            continue;
        if (m.from() == C4 && m.to() == D5)
            pxq = m;
        if (m.from() == F4 && m.to() == D5)
            nxq = m;
    }
    REQUIRE(pxq.is_ok());
    REQUIRE(nxq.is_ok());

    MoveOrdering::MovePicker picker(list, pos, Move::none(), 0);
    REQUIRE(picker.next_move() == pxq); // pawn-takes-queen wins on MVV-LVA
}

TEST_CASE("MovePicker rates killer moves above other quiets", "[ordering][heuristics]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::ItalianGame);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    Move killer = Move::none();
    for (int i = 0; i < list.count; ++i)
        if (!list.moves[i].is_capture() && !list.moves[i].is_promotion())
        {
            killer = list.moves[i];
            break;
        }
    REQUIRE(killer.is_ok());

    Search::killer_moves[0][0] = killer;

    MoveOrdering::MovePicker picker(list, pos, Move::none(), /*ply=*/0);
    REQUIRE(picker.next_move() == killer);
}

TEST_CASE("MovePicker exhausts and returns none()", "[ordering]")
{
    Position pos = Position::from_fen(Fens::StartPos);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    const int n = list.count;

    MoveOrdering::MovePicker picker(list, pos, Move::none(), 0);
    REQUIRE(picker.remaining() == n);

    for (int i = 0; i < n; ++i)
        REQUIRE(picker.next_move().is_ok());

    REQUIRE(picker.remaining() == 0);
    REQUIRE_FALSE(picker.next_move().is_ok());
}

// =============================================================================
// Suite 6 — Heuristics (killers + history)
// =============================================================================
TEST_CASE("update_killers shifts slots correctly", "[heuristics]")
{
    reset_search_state();
    const Move a(E2, E4, QUIET_MOVE);
    const Move b(G1, F3, QUIET_MOVE);

    Search::update_killers(a, /*ply=*/5);
    REQUIRE(Search::killer_moves[5][0] == a);
    REQUIRE_FALSE(Search::killer_moves[5][1].is_ok());

    Search::update_killers(b, 5);
    REQUIRE(Search::killer_moves[5][0] == b);
    REQUIRE(Search::killer_moves[5][1] == a);

    Search::update_killers(b, 5); // re-adding "b" must not shift
    REQUIRE(Search::killer_moves[5][0] == b);
    REQUIRE(Search::killer_moves[5][1] == a);
}

TEST_CASE("update_killers rejects captures and promotions", "[heuristics]")
{
    reset_search_state();
    Search::update_killers(Move(E4, D5, CAPTURE), /*ply=*/3);
    Search::update_killers(Move(E7, E8, QUEEN_PROMO), /*ply=*/3);
    REQUIRE_FALSE(Search::killer_moves[3][0].is_ok());
    REQUIRE_FALSE(Search::killer_moves[3][1].is_ok());
}

TEST_CASE("history updates and penalises correctly", "[heuristics]")
{
    reset_search_state();
    const Move m = Move(E2, E4, QUIET_MOVE);
    const Piece piece = WHITE_PAWN;

    REQUIRE(Search::history_score(m, piece) == 0);

    Search::update_history(m, /*depth=*/4, piece);
    REQUIRE(Search::history_score(m, piece) == 16); // 4 * 4

    Search::penalize_history(m, /*depth=*/2, piece);
    REQUIRE(Search::history_score(m, piece) == 12); // 16 - 4

    Search::penalize_history(m, /*depth=*/10, piece);
    REQUIRE(Search::history_score(m, piece) == 0); // clamped at zero
}

TEST_CASE("history is capped at MAX_HISTORY_SCORE", "[heuristics]")
{
    reset_search_state();
    const Move m = Move(E2, E4, QUIET_MOVE);
    const Piece piece = WHITE_PAWN;

    for (int i = 0; i < 100; ++i)
        Search::update_history(m, 32, piece);

    REQUIRE(Search::history_score(m, piece) == Search::MAX_HISTORY_SCORE);
}

// =============================================================================
// Suite 7 — LMR table
// =============================================================================
TEST_CASE("LMRTable is zero at depth 0 or move 0", "[lmr]")
{
    for (int d = 0; d < MAX_PLY; ++d)
        REQUIRE(Search::LMRTable[d][0] == 0);
    for (int m = 0; m < MAX_MOVES; ++m)
        REQUIRE(Search::LMRTable[0][m] == 0);
}

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
// Suite 8 — Quiescence
// =============================================================================
TEST_CASE("Quiescence: stand-pat >= beta returns beta", "[search][qsearch]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos); // eval == 0
    auto ctx = make_context();

    const Value v = Search::quiescence(pos, /*ply=*/0, /*alpha=*/-100, /*beta=*/-50, ctx);
    REQUIRE(v == -50);
}

TEST_CASE("Quiescence: stand-pat raises alpha", "[search][qsearch]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos); // eval == 0
    auto ctx = make_context();

    const Value v = Search::quiescence(pos, /*ply=*/0, /*alpha=*/-100, /*beta=*/100, ctx);
    REQUIRE(v == 0);
}

TEST_CASE("Quiescence: mate if in check with no evasions", "[search][qsearch][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::BlackCheckmated);
    auto ctx = make_context();

    const Value v = Search::quiescence(pos, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx);
    REQUIRE(v == -VALUE_MATE);
}

// =============================================================================
// Suite 9 — Negamax terminal-node handling
// =============================================================================
TEST_CASE("Negamax recognises checkmate at the root", "[search][negamax][terminal]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::BlackCheckmated);
    auto ctx = make_context();

    const Value v =
        Search::negamax<Search::ROOT>(pos, /*depth=*/1, /*ply=*/0, -VALUE_INFINITE, VALUE_INFINITE, ctx, /*do_null=*/false);
    REQUIRE(v == -VALUE_MATE);
}

TEST_CASE("Negamax recognises stalemate at the root", "[search][negamax][terminal]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::BlackStalemate);
    auto ctx = make_context();

    const Value v = Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_ZERO);
}

TEST_CASE("Negamax finds mate-in-1 at depth 1", "[search][negamax][mate]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::WhiteMateIn1);
    auto ctx = make_context();

    const Value v = Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_MATE - 1);
    REQUIRE(ctx.info.best_move.is_ok());
}

// =============================================================================
// Suite 10 — Node-type template equivalence
// =============================================================================
TEST_CASE("ROOT, PV, NON_PV agree on a full-window search", "[search][negamax][template]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::ItalianGame);

    auto ctx_r = make_context();
    const Value v_root = Search::negamax<Search::ROOT>(pos, 2, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx_r, false);

    auto ctx_p = make_context();
    const Value v_pv = Search::negamax<Search::PV>(pos, 2, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx_p, false);

    auto ctx_n = make_context();
    const Value v_npv = Search::negamax<Search::NON_PV>(pos, 2, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx_n, false);

    REQUIRE(v_root == v_pv);
    REQUIRE(v_pv == v_npv);
}

// =============================================================================
// Suite 11 — Stop signal
// =============================================================================
TEST_CASE("Negamax returns VALUE_ZERO when stop is set", "[search][negamax][stop]")
{
    reset_search_state();
    Search::Signals_stop.store(true, std::memory_order_relaxed);

    Position pos = Position::from_fen(Fens::ItalianGame);
    auto ctx = make_context();

    const Value v = Search::negamax<Search::ROOT>(pos, 5, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_ZERO);

    Search::Signals_stop.store(false, std::memory_order_relaxed); // cleanup
}

// =============================================================================
// Suite 12 — Transposition table
// =============================================================================
TEST_CASE("TT store/probe round-trips all fields", "[tt]")
{
    Search::TT.resize(1);
    Search::TT.clear();

    const Key k = 0x123456789ABCDEF0ULL;
    const Move m(E2, E4, DOUBLE_PAWN_PUSH);

    Search::TT.store(k, /*ply=*/0, /*depth=*/5, /*score=*/42, Search::TT_EXACT, m);

    Search::TTEntry e;
    REQUIRE(Search::TT.probe(k, 0, e));
    REQUIRE(e.key == k);
    REQUIRE(e.score == 42);
    REQUIRE(e.depth == 5);
    REQUIRE(e.flag == Search::TT_EXACT);
    REQUIRE(e.best_move == m);
}

TEST_CASE("TT probe misses when key is absent", "[tt]")
{
    Search::TT.resize(1);
    Search::TT.clear();

    Search::TTEntry e;
    REQUIRE_FALSE(Search::TT.probe(0xDEADBEEFCAFEBABEULL, 0, e));
}

TEST_CASE("TT adjusts mate scores on store and probe", "[tt][mate]")
{
    Search::TT.resize(1);
    Search::TT.clear();

    const Key k = 0xCAFEBABEDEADBEEFULL;
    const Move m(E7, E8, QUEEN_PROMO);

    Search::TT.store(k, 0, 10, VALUE_MATE - 5, Search::TT_EXACT, m);

    Search::TTEntry e0;
    REQUIRE(Search::TT.probe(k, 0, e0));
    REQUIRE(e0.score == VALUE_MATE - 5);

    Search::TTEntry e5;
    REQUIRE(Search::TT.probe(k, 5, e5));
    REQUIRE(e5.score == VALUE_MATE - 10);
}

TEST_CASE("TT adjusts losing mate scores symmetrically", "[tt][mate]")
{
    Search::TT.resize(1);
    Search::TT.clear();

    const Key k = 0x0F0F0F0F0F0F0F0FULL;
    const Move m(A1, A8, QUEEN_PROMO_CAP);

    Search::TT.store(k, 0, 10, -VALUE_MATE + 5, Search::TT_EXACT, m);

    Search::TTEntry e;
    REQUIRE(Search::TT.probe(k, 5, e));
    REQUIRE(e.score == -VALUE_MATE + 10);
}

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
    REQUIRE(e.score == 20);
    REQUIRE(e.best_move == deep_m);
}

TEST_CASE("TT refuses to lower an existing depth for the same key", "[tt][replacement]")
{
    Search::TT.resize(1);
    Search::TT.clear();

    const Key k = 0x55555555AAAAAAAAULL;
    const Move deep_m(A2, A4, DOUBLE_PAWN_PUSH);
    const Move shallow_m(A2, A3, QUIET_MOVE);

    Search::TT.store(k, 0, 8, 20, Search::TT_EXACT, deep_m);
    Search::TT.store(k, 0, 2, 5, Search::TT_EXACT, shallow_m);

    Search::TTEntry e;
    REQUIRE(Search::TT.probe(k, 0, e));
    REQUIRE(e.depth == 8);
    REQUIRE(e.score == 20);
    REQUIRE(e.best_move == deep_m);
}

TEST_CASE("TT hashfull returns 0 on a fresh table and >0 after inserts", "[tt][hashfull]")
{
    Search::TT.resize(1);
    Search::TT.clear();
    REQUIRE(Search::TT.hashfull() == 0);

    const Move m(E2, E4, DOUBLE_PAWN_PUSH);
    for (uint64_t i = 0; i < 5000; ++i)
    {
        const Key k = (i * 0x9E3779B97F4A7C15ULL) | 1ULL; // never zero
        Search::TT.store(k, 0, 5, static_cast<Value>(i), Search::TT_EXACT, m);
    }
    REQUIRE(Search::TT.hashfull() > 0);
}

// =============================================================================
// Suite 13 — Repetition detection
// =============================================================================
TEST_CASE("is_repetition: initial position is not a repetition", "[repetition]")
{
    const Position pos = Position::from_fen(Fens::StartPos);
    REQUIRE_FALSE(is_repetition(pos));
}

TEST_CASE("is_repetition: knight shuffle returns to the start position", "[repetition]")
{
    Position pos = Position::from_fen(Fens::StartPos);

    auto play = [&](Square from, Square to)
    {
        pos.make_move(Move(from, to, QUIET_MOVE));
    };

    play(G1, F3); // 1. Nf3
    REQUIRE_FALSE(is_repetition(pos));

    play(G8, F6); // 1... Nf6
    REQUIRE_FALSE(is_repetition(pos));

    play(F3, G1); // 2. Ng1
    REQUIRE_FALSE(is_repetition(pos));

    play(F6, G8); // 2... Ng8 — back to start
    REQUIRE(is_repetition(pos));
}

// =============================================================================
// Suite 14 — start_search (UCI loop) smoke test
// =============================================================================
TEST_CASE("start_search emits a legal bestmove and does not corrupt the position", "[search][uci]")
{
    reset_search_state();

    Position pos = Position::from_fen(Fens::StartPos);
    const Key key_before = pos.key;

    SearchLimits limits{};
    limits.depth = 2; // shallow: milliseconds
    limits.wtime = 1000;
    limits.btime = 1000;
    limits.winc = 0;
    limits.binc = 0;
    limits.movetime = 0;
    limits.infinite = false;

    std::ostringstream buffer;
    std::streambuf* old_buf = std::cout.rdbuf(buffer.rdbuf());

    Search::start_search(pos, limits);

    std::cout.rdbuf(old_buf);
    const std::string output = buffer.str();

    REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("bestmove"));

    REQUIRE(pos.key == key_before);
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.ply == 0);
}

// =============================================================================
// Suite 15 — Castling rights tracking through make/unmake
// =============================================================================
TEST_CASE("Moving a king clears its side's castling rights", "[position][castling]")
{
    Position pos = Position::from_fen(Fens::StartPos);
    REQUIRE((pos.castlingRights & 0x03) == 0x03); // both white rights set

    pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
    pos.make_move(Move(E7, E5, DOUBLE_PAWN_PUSH));
    pos.make_move(Move(E1, E2, QUIET_MOVE));

    REQUIRE((pos.castlingRights & 0x03) == 0x00); // white rights gone
    REQUIRE((pos.castlingRights & 0x0C) == 0x0C); // black rights untouched

    pos.unmake_move(Move(E1, E2, QUIET_MOVE));
    REQUIRE(pos.castlingRights == 0x0F);
}

TEST_CASE("Moving a rook clears only that rook's castling right", "[position][castling]")
{
    Position pos = Position::from_fen(Fens::StartPos);

    pos.make_move(Move(A2, A4, DOUBLE_PAWN_PUSH));
    pos.make_move(Move(A7, A5, DOUBLE_PAWN_PUSH));
    pos.make_move(Move(A1, A3, QUIET_MOVE));

    REQUIRE((pos.castlingRights & 0x01) == 0x01); // white K still set
    REQUIRE((pos.castlingRights & 0x02) == 0x00); // white Q cleared
    REQUIRE((pos.castlingRights & 0x0C) == 0x0C); // black both still set
}

// =============================================================================
// Suite 16 — Killer-move regression (formerly the mismatch diagnostic)
//
// This test verifies that the search and the MovePicker share the same killer
// storage. It would fail under the old architecture where search.cpp wrote to
// SearchContext::killer_moves while move_ordering.cpp read from the global
// Search::killer_moves.
// =============================================================================
TEST_CASE("Killers stored by the search are visible to the MovePicker", "[ordering][heuristics][killer-mismatch]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::ItalianGame);
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);
    REQUIRE(list.count > 0);

    // Pick a quiet move and store it as a killer via the search helper.
    Move candidate = Move::none();
    for (int i = 0; i < list.count; ++i)
        if (!list.moves[i].is_capture() && !list.moves[i].is_promotion())
        {
            candidate = list.moves[i];
            break;
        }
    REQUIRE(candidate.is_ok());

    // Store through the same path the search uses at a cutoff.
    Search::update_killers(candidate, /*ply=*/0);

    // The MovePicker must now prioritize that killer.
    MoveOrdering::MovePicker picker(list, pos, Move::none(), /*ply=*/0);
    REQUIRE(picker.next_move() == candidate);

    // And the killer table must contain exactly what we stored.
    REQUIRE(Search::killer_moves[0][0] == candidate);
}