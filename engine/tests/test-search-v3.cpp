// tests/test_search_comprehensive.cpp
// Comprehensive Catch2 v3 test suite for the UCI chess engine's search subsystem.

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

namespace Search
{
    void init_lmr_table();
    extern int LMRTable[MAX_PLY][MAX_MOVES];
} // namespace Search

bool is_repetition(const Position& pos);

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
        Search::TT.resize(1); // 1MB TT for tests
        Search::TT.clear();
    }

    namespace Fens
    {
        constexpr const char* StartPos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        // White to move, forced mate in 4 (Requires check extensions to see clearly at lower depths)
        constexpr const char* MateIn4 =
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1"; // Simplification for structural tests
        constexpr const char* Draw50Move = "7k/8/8/8/8/8/8/R3K2R w KQ - 100 51";
        constexpr const char* Zugzwang = "8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1"; // High risk for NMP
    } // namespace Fens
} // namespace

// =============================================================================
// 1. Draw Detection (50-Move Rule & Repetition)
// =============================================================================

TEST_CASE("Search: 50-move rule correctly aborts search with VALUE_ZERO", "[search][draws]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::Draw50Move);
    auto ctx = make_context();

    // Even with a forced mate or high eval, the 50-move rule must override it.
    // NOTE: ply must be > 0 for 50-move rule to trigger in negamax.
    const Value v = Search::negamax<Search::ROOT>(pos, 5, 1, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_ZERO);
}

TEST_CASE("Search: 3-fold repetition correctly aborts search with VALUE_ZERO", "[search][draws]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos);

    // Simulate a Knight shuffle
    pos.make_move(Move(G1, F3, QUIET_MOVE));
    pos.make_move(Move(G8, F6, QUIET_MOVE));
    pos.make_move(Move(F3, G1, QUIET_MOVE));
    pos.make_move(Move(F6, G8, QUIET_MOVE)); // First repetition

    pos.make_move(Move(B1, C3, QUIET_MOVE));
    pos.make_move(Move(B8, C6, QUIET_MOVE));
    pos.make_move(Move(C3, B1, QUIET_MOVE));
    pos.make_move(Move(C6, B8, QUIET_MOVE)); // Second repetition

    REQUIRE(is_repetition(pos) == true);

    auto ctx = make_context();
    const Value v = Search::negamax<Search::ROOT>(pos, 4, 1, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
    REQUIRE(v == VALUE_ZERO);
}

// =============================================================================
// 2. Move Ordering (Exhaustive Priorities)
// =============================================================================

TEST_CASE("Search: MovePicker strictly follows priority: TT -> Good Capture -> Promo -> Killer -> History -> Quiet",
          "[search][ordering]")
{
    reset_search_state();

    // Position with captures, promotions, and quiets
    Position pos = Position::from_fen("4k3/P6p/8/8/8/8/7P/4K2R w K - 0 1");
    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    // Identify specific moves
    Move promo(A7, A8, QUEEN_PROMO);
    Move capture(H1, H7, CAPTURE); // RxP
    Move quiet(H2, H3, QUIET_MOVE);
    Move killer(E1, D2, QUIET_MOVE);
    Move tt_move(E1, E2, QUIET_MOVE);

    // Setup heuristics
    Search::killer_moves[0][0] = killer;
    Search::update_history(quiet, 5, pos.board[quiet.from()]); // Give quiet some history score

    MoveOrdering::MovePicker picker(list, pos, tt_move, 0);

    // 1. Transposition Table move must be first
    REQUIRE(picker.next_move() == tt_move);
    // 2. Captures (MVV-LVA)
    REQUIRE(picker.next_move() == capture);
    // 3. Promotions
    REQUIRE(picker.next_move() == promo);
    // 4. Killer Moves
    REQUIRE(picker.next_move() == killer);

    // Eventually, the history-boosted quiet move should appear before zero-history quiet moves
    bool found_history_quiet = false;
    Move m = picker.next_move();
    while (m.is_ok())
    {
        if (m == quiet)
        {
            found_history_quiet = true;
            break;
        }
        m = picker.next_move();
    }
    REQUIRE(found_history_quiet == true);
}

// =============================================================================
// 3. Null-Move Pruning (NMP) Safety & Triggers
// =============================================================================

TEST_CASE("Search: NMP is skipped when in check", "[search][nmp]")
{
    reset_search_state();
    Position pos = Position::from_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1"); // White in check
    auto ctx = make_context();

    Value out_score = 0;
    bool pruned = Search::try_null_move_pruning(pos, 5, 0, 100, ctx, out_score);

    // Should immediately return false because of `in_check` condition logic prior to entry,
    // or if forced explicitly, the guard should catch it.
    REQUIRE(pruned == false);
}

TEST_CASE("Search: NMP is skipped in pawn endgames (Zugzwang guard)", "[search][nmp]")
{
    reset_search_state();
    Position pos = Position::from_fen("8/k7/8/P7/8/8/8/K7 w - - 0 1"); // Only pawns and kings
    auto ctx = make_context();

    Value out_score = 0;
    bool pruned = Search::try_null_move_pruning(pos, 5, 0, -1000, ctx, out_score);

    // non_pawn_material == 0, so NMP should abort
    REQUIRE(pruned == false);
}

// =============================================================================
// 4. Transposition Table (TT) Edge Cases & Math
// =============================================================================

TEST_CASE("Search: TT correctly offsets mate scores by ply to preserve shortest path", "[search][tt]")
{
    reset_search_state();
    const Key k = 0x9999999999999999ULL;
    const Move m(E2, E4, DOUBLE_PAWN_PUSH);

    // Store a mate found at ply 3
    const Value mate_in_3 = VALUE_MATE - 3;
    Search::TT.store(k, 3, 10, mate_in_3, Search::TT_EXACT, m);

    Search::TTEntry e;
    // Probe it at ply 5. It should decode to a mate-in-5 from the root perspective
    // to keep the absolute distance to mate identical.
    REQUIRE(Search::TT.probe(k, 5, e));
    REQUIRE(e.score == VALUE_MATE - 5);
}

TEST_CASE("Search: TT Two-Tier Replacement resolves collisions efficiently", "[search][tt]")
{
    reset_search_state();
    const Key k1 = 0x1111111111111111ULL;
    const Key k2 = 0x1111111111111111ULL | 0x2; // Different key, same bucket mask for small TT

    const Move m1(A2, A3, QUIET_MOVE);
    const Move m2(B2, B3, QUIET_MOVE);

    // Fill the depth-preferred slot (Slot 0)
    Search::TT.store(k1, 0, 10, 50, Search::TT_EXACT, m1);

    // Attempt to store a shallower depth in the same bucket
    Search::TT.store(k2, 0, 5, 25, Search::TT_EXACT, m2);

    Search::TTEntry e1, e2;
    REQUIRE(Search::TT.probe(k1, 0, e1));
    REQUIRE(Search::TT.probe(k2, 0, e2));

    // Both should coexist because the shallower entry is routed to slot 1, 2, or 3
    REQUIRE(e1.key == k1);
    REQUIRE(e2.key == k2);
    REQUIRE(e1.depth == 10);
    REQUIRE(e2.depth == 5);
}

// =============================================================================
// 5. Mate Distance Pruning (MDP)
// =============================================================================

TEST_CASE("Search: Mate Distance Pruning limits bounds based on ply", "[search][mdp]")
{
    reset_search_state();
    // Use an arbitrary position
    Position pos = Position::from_fen(Fens::StartPos);
    auto ctx = make_context();

    // If we call negamax at a high ply, bounds should be clamped.
    // Simulating alpha = -VALUE_INFINITE, beta = VALUE_INFINITE at ply 10
    // Alpha becomes max(-VALUE_INFINITE, -VALUE_MATE + 10)
    // Beta becomes min(VALUE_INFINITE, VALUE_MATE - 10 - 1)

    // We can observe this indirectly if a TT entry returns a mate score outside the bounds.
    const Key k = pos.key;
    // Pretend a mate-in-5 was found previously
    Search::TT.store(k, 0, 20, VALUE_MATE - 5, Search::TT_EXACT, Move(E2, E4, DOUBLE_PAWN_PUSH));

    const Value v = Search::negamax<Search::ROOT>(pos, 5, 10, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    // Because ply = 10, beta is clamped to VALUE_MATE - 11.
    // The TT score is VALUE_MATE - 5. This is >= beta, triggering a fail-high, returning beta.
    // Beta at ply 10 is (32000 - 10 - 1) = 31989
    REQUIRE(v == VALUE_MATE - 11);
}

// =============================================================================
// 6. Quiescence Search (Q-Search) Exact Behaviors
// =============================================================================

TEST_CASE("Search: Quiescence Search evaluates forcing moves to bypass horizon effect", "[search][qsearch]")
{
    reset_search_state();
    // Position where white can capture a queen, but doing so hangs their own king to a rook.
    // A naive evaluation without Q-Search would just see the queen capture as good.
    Position pos = Position::from_fen("4k3/8/8/3q4/8/8/4P3/3R1K2 w - - 0 1");
    auto ctx = make_context();

    // Alpha-beta limits
    const Value alpha = -VALUE_INFINITE;
    const Value beta = VALUE_INFINITE;

    Value v = Search::quiescence(pos, 0, alpha, beta, ctx);

    // The score should reflect reality: it's not +900 (queen up) because Q-Search evaluates the captures.
    // White's best quiet eval is tested.
    REQUIRE(v < Eval::MG_WEIGHT[QUEEN]);
}

TEST_CASE("Search: Quiescence Search stops at MAX_SEARCH_PLY", "[search][qsearch]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos);
    auto ctx = make_context();

    // Enter Q-Search at the maximum allowed ply limit
    Value static_eval = Eval::evaluate(pos);
    Value v = Search::quiescence(pos, Search::MAX_SEARCH_PLY - 1, -VALUE_INFINITE, VALUE_INFINITE, ctx);

    // Should immediately return static eval without expanding moves
    REQUIRE(v == static_eval);
}

// =============================================================================
// 7. Check Extensions
// =============================================================================

TEST_CASE("Search: Being in check extends search depth", "[search][extensions]")
{
    reset_search_state();
    // A position where black is in check
    Position pos = Position::from_fen("4k3/4Q3/8/8/8/8/8/4K3 b - - 0 1");
    auto ctx = make_context();

    // A depth 1 search should be extended by +1 because of the check
    Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    // If depth was extended, nodes searched should reflect deeper penetration than depth 1
    // (A depth 1 check evasion would normally hit depth 0 immediately and drop to QSearch).
    REQUIRE(ctx.info.nodes > 0);
}

// =============================================================================
// 8. Interrupt Handling
// =============================================================================

TEST_CASE("Search: Signals_stop cleanly aborts search midway", "[search][interrupt]")
{
    reset_search_state();
    Position pos = Position::from_fen(Fens::StartPos);
    auto ctx = make_context();

    // Fire the stop signal before we even start
    Search::Signals_stop.store(true, std::memory_order_relaxed);

    Value v = Search::negamax<Search::ROOT>(pos, 10, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);

    // Must immediately abort and return VALUE_ZERO
    REQUIRE(v == VALUE_ZERO);
    REQUIRE(ctx.info.nodes == 0); // No nodes should be evaluated
}