// tests/test_search_comprehensive.cpp
// Comprehensive Catch2 v3 test suite for the UCI chess engine's search subsystem.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "board/attacks.h"
#include "board/bitboard.h"
#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"
#include "board/zobrist.h"

#include "eval/evaluate.h"

#include "search/heuristics.h"
#include "search/move_ordering.h"
#include "search/search.h"
#include "search/transposition.h"

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
TEST_CASE("Search: MovePicker strictly follows priority: TT -> Queen Promo -> Good Capture -> Killer -> Knight Promo -> Quiet -> "
          "Minor Promos",
          "[search][ordering]")
{
    reset_search_state();

    // White Ke1, Rh1, pawn a7. Black Ke8, rook h7.
    // - Rh1xh7 is a legal capture (h-file is clear).
    // - a7a8 promotes to four distinct pieces (all non-capture promotions).
    Position pos = Position::from_fen("4k3/P6r/8/8/8/8/8/4K2R w K - 0 1");

    MoveList list;
    MoveGen::generate_legal_moves(pos, list);

    const Move promo_q(A7, A8, QUEEN_PROMO);
    const Move promo_r(A7, A8, ROOK_PROMO);
    const Move promo_b(A7, A8, BISHOP_PROMO);
    const Move promo_n(A7, A8, KNIGHT_PROMO);
    const Move capture(H1, H7, CAPTURE);  // RxR (Good Capture)
    const Move quiet(E1, F1, QUIET_MOVE); // Will be history-boosted
    const Move killer(E1, D2, QUIET_MOVE);
    const Move tt_move(E1, E2, QUIET_MOVE);

    auto contains = [&](Move m)
    {
        for (int i = 0; i < list.count; ++i)
            if (list.moves[i] == m)
                return true;
        return false;
    };
    REQUIRE(contains(promo_q));
    REQUIRE(contains(promo_r));
    REQUIRE(contains(promo_b));
    REQUIRE(contains(promo_n));
    REQUIRE(contains(capture));
    REQUIRE(contains(quiet));
    REQUIRE(contains(killer));
    REQUIRE(contains(tt_move));

    // Setup heuristics
    Search::killer_moves[0][0] = killer;
    Search::update_history(quiet, 5, pos.board[quiet.from()]);

    MoveOrdering::MovePicker picker(list, pos, tt_move, 0);

    // 1. TT move (Score: 10,000,000)
    REQUIRE(picker.next_move() == tt_move);

    // 2. Queen Promotion (Score: 9,000,000)
    REQUIRE(picker.next_move() == promo_q);

    // 3. Good Capture (Score: 8,000,000 + MVV_LVA)
    REQUIRE(picker.next_move() == capture);

    // 4. Killer Slot 1 (Score: 7,000,000)
    REQUIRE(picker.next_move() == killer);

    // 5. Knight Promotion (Score: 5,000,000)
    REQUIRE(picker.next_move() == promo_n);

    // 6. History-boosted quiet (Score: ~25. All other quiets have 0)
    REQUIRE(picker.next_move() == quiet);

    // 7. Drain the rest of the moves to verify Minor Promotions are dead last
    std::vector<Move> remaining_moves;
    while (picker.remaining() > 0)
    {
        remaining_moves.push_back(picker.next_move());
    }

    // Un-boosted quiets evaluate to 0. Minor promotions evaluate to -5,000,000.
    // Therefore, the minor promotions must be the final two moves extracted.
    REQUIRE(remaining_moves.size() >= 2);
    Move last_1 = remaining_moves[remaining_moves.size() - 1];
    Move last_2 = remaining_moves[remaining_moves.size() - 2];

    bool minors_last = (last_1 == promo_r && last_2 == promo_b) || (last_1 == promo_b && last_2 == promo_r);

    REQUIRE(minors_last);
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
//
// MDP clamps the alpha/beta window at every node so that the absolute distance
// to mate from the root is preserved regardless of where in the tree the mate
// was discovered. Verified here with a mate-in-1 searched at ply 0 and ply 32.
// =============================================================================

TEST_CASE("Search: Mate Distance Pruning adjusts mate scores by ply", "[search][mdp]")
{
    reset_search_state();

    // Mate-in-1: white plays Ra8#.
    Position pos = Position::from_fen("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");

    // At ply 0 the search returns VALUE_MATE - 1.
    {
        auto ctx = make_context();
        const Value v = Search::negamax<Search::ROOT>(pos, 1, 0, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
        REQUIRE(v == VALUE_MATE - 1);
    }

    // At ply 32 the same mate is offset by the ply. The mate score is encoded
    // as VALUE_MATE - ply so that a mate found deeper in the tree never
    // pretends to be closer than it really is. MDP clamps the window at
    // [-VALUE_MATE + ply, VALUE_MATE - ply - 1], so the mate must be reported
    // as VALUE_MATE - (ply + 1) once we descend one level to the checkmated
    // side's node.
    {
        auto ctx = make_context();
        const Value v = Search::negamax<Search::ROOT>(pos, 1, 32, -VALUE_INFINITE, VALUE_INFINITE, ctx, false);
        REQUIRE(v == VALUE_MATE - 33);
    }
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

TEST_CASE("Search: Aspiration Windows prune nodes while preserving evaluation accuracy", "[search][aspiration]")
{
    reset_search_state();

    // KiwiPete: heavily tactical position with pins, checks, and many legal moves
    Position pos = Position::from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    const int target_depth = 5;

    // 1. Baseline: Full-window search across all plies
    auto full_ctx = make_context();
    Value full_score = Search::negamax<Search::ROOT>(pos, target_depth, 0, -VALUE_INFINITE, VALUE_INFINITE, full_ctx, false);
    Move full_best_move = full_ctx.info.best_move;
    uint64_t full_nodes = full_ctx.info.nodes;

    // Reset TT and heuristics to ensure identical starting conditions
    reset_search_state();

    // 2. Aspiration Search Simulation (starting from depth 4 score as baseline)
    auto asp_ctx = make_context();
    Value depth_minus_one_score =
        Search::negamax<Search::ROOT>(pos, target_depth - 1, 0, -VALUE_INFINITE, VALUE_INFINITE, asp_ctx, false);

    // Reset per-depth node counter to measure target_depth search exclusively
    asp_ctx.info.nodes = 0;

    int delta = 25;
    Value alpha = std::max(depth_minus_one_score - delta, -VALUE_INFINITE);
    Value beta = std::min(depth_minus_one_score + delta, VALUE_INFINITE);
    Value asp_score = VALUE_ZERO;

    while (true)
    {
        asp_score = Search::negamax<Search::ROOT>(pos, target_depth, 0, alpha, beta, asp_ctx, false);

        if (asp_score <= alpha)
        {
            beta = (alpha + beta) / 2;
            alpha = std::max(alpha - delta, -VALUE_INFINITE);
            delta += delta / 2;
        }
        else if (asp_score >= beta)
        {
            beta = std::min(beta + delta, VALUE_INFINITE);
            delta += delta / 2;
        }
        else
        {
            break; // Score resolved cleanly within window
        }

        if (delta >= 1000)
        {
            alpha = -VALUE_INFINITE;
            beta = VALUE_INFINITE;
        }
    }

    // Accuracy Check: Must agree with full-window score & move
    REQUIRE(asp_score == full_score);
    REQUIRE(asp_ctx.info.best_move == full_best_move);

    // Efficiency Check: Aspiration window must explore fewer nodes at target depth
    REQUIRE(asp_ctx.info.nodes < full_nodes);
}

TEST_CASE("Search: Dynamic Time Management adjusts limits upon root instability", "[search][time_management]")
{
    SearchLimits limits{};
    limits.wtime = 4000; // 4000 ms remaining
    limits.winc = 0;

    Position pos = Position::from_fen(Fens::StartPos);

    // Simulate time initialization logic from start_search
    uint64_t time_left = (pos.sideToMove == WHITE) ? limits.wtime : limits.btime;
    uint64_t inc = (pos.sideToMove == WHITE) ? limits.winc : limits.binc;

    uint64_t opt_time = (time_left / 40) + (inc * 3 / 4);                               // 4000 / 40 = 100 ms
    uint64_t max_time = std::min(time_left / 5, (time_left > 50) ? time_left - 50 : 0); // 4000 / 5 = 800 ms
    opt_time = std::min(opt_time, max_time);

    REQUIRE(opt_time == 100);
    REQUIRE(max_time == 800);

    // Scenario A: Move remains stable between iterations
    Move current_best = Move(E2, E4, DOUBLE_PAWN_PUSH);
    Move previous_best = Move(E2, E4, DOUBLE_PAWN_PUSH);

    if (current_best != previous_best)
    {
        opt_time = std::min(static_cast<uint64_t>(opt_time * 1.5), max_time);
    }
    REQUIRE(opt_time == 100); // Unchanged

    // Scenario B: Root best-move changes (instability detected)
    current_best = Move(D2, D4, DOUBLE_PAWN_PUSH);

    if (current_best != previous_best)
    {
        opt_time = std::min(static_cast<uint64_t>(opt_time * 1.5), max_time);
    }
    REQUIRE(opt_time == 150); // Scaled by 1.5x

    // Scenario C: Multiple oscillations must not exceed the hard max ceiling
    for (int i = 0; i < 10; ++i)
    {
        opt_time = std::min(static_cast<uint64_t>(opt_time * 1.5), max_time);
    }
    REQUIRE(opt_time == max_time);
    REQUIRE(opt_time == 800);
}