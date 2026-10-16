// gui/tests/ChessGameTest.cpp
//
// Standalone unit test for ChessGame: history navigation and result().
// No SFML. No ImGui. No Application.

#include "game/ChessGame.hpp"
#include "game/Board.hpp"
#include "game/GameResult.hpp"
#include "board/types.h"

#include "board/attacks.h"
#include "board/zobrist.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace test
{
    int g_failures = 0;
    int g_checks = 0;

    void expectTrue(bool cond, const std::string& label)
    {
        ++g_checks;
        if (cond)
        {
            std::cout << "  [PASS] " << label << "\n";
        }
        else
        {
            ++g_failures;
            std::cout << "  [FAIL] " << label << "\n";
        }
    }

    void expectEq(std::size_t actual, std::size_t expected, const std::string& label)
    {
        ++g_checks;
        if (actual == expected)
        {
            std::cout << "  [PASS] " << label << "\n";
        }
        else
        {
            ++g_failures;
            std::cout << "  [FAIL] " << label << "\n"
                      << "         expected: " << expected << "\n"
                      << "         actual:   " << actual << "\n";
        }
    }

    void expectEq(const std::string& actual, const std::string& expected, const std::string& label)
    {
        ++g_checks;
        if (actual == expected)
        {
            std::cout << "  [PASS] " << label << "\n";
        }
        else
        {
            ++g_failures;
            std::cout << "  [FAIL] " << label << "\n"
                      << "         expected: \"" << expected << "\"\n"
                      << "         actual:   \"" << actual << "\"\n";
        }
    }

    int summary()
    {
        std::cout << "\n=== " << (g_checks - g_failures) << "/" << g_checks << " checks passed ===\n";
        return g_failures == 0 ? 0 : 1;
    }
} // namespace test

namespace
{

    Square parseSquare(const std::string& s)
    {
        if (s.size() != 2)
            return SQ_NONE;
        const int file = s[0] - 'a';
        const int rank = s[1] - '1';
        if (file < 0 || file > 7 || rank < 0 || rank > 7)
            return SQ_NONE;
        return static_cast<Square>(rank * 8 + file);
    }

    bool play(ChessGame& g, const std::string& fromTo)
    {
        const Square from = parseSquare(fromTo.substr(0, 2));
        const Square to = parseSquare(fromTo.substr(2, 2));
        return g.play(from, to).has_value();
    }

    std::string joinSan(const std::vector<MoveRecord>& records)
    {
        std::string out;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            if (i)
                out += ' ';
            out += records[i].san;
        }
        return out;
    }

    std::string describe(const GameResult& r)
    {
        switch (r.kind)
        {
            case GameResultKind::Ongoing:
                return "Ongoing";
            case GameResultKind::Checkmate:
                return "Checkmate";
            case GameResultKind::Stalemate:
                return "Stalemate";
            case GameResultKind::FiftyMove:
                return "FiftyMove";
            case GameResultKind::Threefold:
                return "Threefold";
            case GameResultKind::InsufficientMaterial:
                return "InsufficientMaterial";
        }
        return "?";
    }

} // namespace

int main()
{
    init_attacks();
    Zobrist::init();

    // ------------------------------------------------------------------
    std::cout << "\n[Initial state]\n";
    {
        ChessGame g;
        test::expectEq(g.cursor(), std::size_t{0}, "fresh cursor == 0");
        test::expectEq(g.totalMoves(), std::size_t{0}, "fresh totalMoves == 0");
        test::expectEq(g.historyToCursor().size(), std::size_t{0}, "fresh historyToCursor is empty");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[play appends]\n";
    {
        ChessGame g;
        test::expectTrue(play(g, "e2e4"), "play e2e4 returns true");
        test::expectEq(g.cursor(), std::size_t{1}, "cursor == 1");
        test::expectEq(g.totalMoves(), std::size_t{1}, "totalMoves == 1");
        test::expectEq(g.history()[0].san, std::string{"e4"}, "SAN of first move");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[play rejects illegal]\n";
    {
        ChessGame g;
        test::expectTrue(!play(g, "e2e5"), "e2e5 is rejected");
        test::expectEq(g.cursor(), std::size_t{0}, "cursor still 0");
        test::expectEq(g.totalMoves(), std::size_t{0}, "totalMoves still 0");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[undo / redo one step]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        play(g, "g1f3");
        test::expectEq(g.cursor(), std::size_t{3}, "after 3 moves, cursor == 3");

        test::expectTrue(g.undo(), "undo returns true");
        test::expectEq(g.cursor(), std::size_t{2}, "cursor == 2");
        test::expectEq(g.totalMoves(), std::size_t{3}, "totalMoves unchanged == 3");

        test::expectTrue(g.redo(), "redo returns true");
        test::expectEq(g.cursor(), std::size_t{3}, "cursor == 3 again");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[undo at start, redo at end]\n";
    {
        ChessGame g;
        test::expectTrue(!g.undo(), "undo at cursor 0 returns false");

        play(g, "e2e4");
        test::expectTrue(!g.redo(), "redo at end returns false");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[gotoStart / gotoEnd]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        play(g, "g1f3");

        g.gotoStart();
        test::expectEq(g.cursor(), std::size_t{0}, "gotoStart -> 0");

        g.gotoEnd();
        test::expectEq(g.cursor(), std::size_t{3}, "gotoEnd -> 3");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[gotoPly clamps]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");

        g.gotoPly(1);
        test::expectEq(g.cursor(), std::size_t{1}, "gotoPly(1) -> 1");

        g.gotoPly(99);
        test::expectEq(g.cursor(), std::size_t{2}, "gotoPly(99) clamps to 2");

        g.gotoPly(0);
        test::expectEq(g.cursor(), std::size_t{0}, "gotoPly(0) -> 0");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[historyToCursor vs history]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        play(g, "g1f3");
        g.undo();

        test::expectEq(g.historyToCursor().size(), std::size_t{2}, "historyToCursor size == 2");
        test::expectEq(g.history().size(), std::size_t{3}, "history size == 3");
        test::expectEq(joinSan(g.historyToCursor()), std::string{"e4 e5"}, "historyToCursor SAN == e4 e5");
        test::expectEq(joinSan(g.history()), std::string{"e4 e5 Nf3"}, "history SAN == e4 e5 Nf3");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[play forks the redo tail]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        play(g, "g1f3");
        g.gotoStart();

        test::expectTrue(play(g, "d2d4"), "play d2d4 after gotoStart");
        test::expectEq(g.cursor(), std::size_t{1}, "cursor == 1");
        test::expectEq(g.totalMoves(), std::size_t{1}, "totalMoves == 1 (tail discarded)");
        test::expectEq(joinSan(g.history()), std::string{"d4"}, "history is just d4 now");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[illegal play does NOT fork the redo tail]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        g.gotoStart();

        test::expectTrue(!play(g, "e2e5"), "e2e5 illegal returns false");
        test::expectEq(g.cursor(), std::size_t{0}, "cursor still 0");
        test::expectEq(g.totalMoves(), std::size_t{2}, "totalMoves still 2 (tail preserved)");

        test::expectTrue(g.redo(), "redo #1");
        test::expectTrue(g.redo(), "redo #2");
        test::expectEq(g.cursor(), std::size_t{2}, "back at end");
        test::expectEq(joinSan(g.history()), std::string{"e4 e5"}, "tail preserved: e4 e5");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[board state follows the cursor]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");

        test::expectEq(g.pieceAt(parseSquare("e4")) != PIECE_NONE, true, "e4 occupied at cursor 2");
        test::expectEq(g.pieceAt(parseSquare("e5")) != PIECE_NONE, true, "e5 occupied at cursor 2");

        g.gotoPly(0);

        test::expectEq(g.pieceAt(parseSquare("e4")) == PIECE_NONE, true, "e4 empty at cursor 0");
        test::expectEq(g.pieceAt(parseSquare("e5")) == PIECE_NONE, true, "e5 empty at cursor 0");
        test::expectEq(g.pieceAt(parseSquare("e2")) != PIECE_NONE, true, "e2 pawn is home at cursor 0");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[loadFEN / reset clear history]\n";
    {
        ChessGame g;
        play(g, "e2e4");
        play(g, "e7e5");
        g.reset();
        test::expectEq(g.cursor(), std::size_t{0}, "reset cursor == 0");
        test::expectEq(g.totalMoves(), std::size_t{0}, "reset totalMoves == 0");

        play(g, "e2e4");
        g.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        test::expectEq(g.cursor(), std::size_t{0}, "loadFEN cursor == 0");
        test::expectEq(g.totalMoves(), std::size_t{0}, "loadFEN totalMoves == 0");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: ongoing at start]\n";
    {
        ChessGame g;
        test::expectEq(describe(g.result()), std::string{"Ongoing"}, "start position is Ongoing");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: checkmate]\n";
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");

        const GameResult r = g.result();
        test::expectEq(describe(r), std::string{"Checkmate"}, "Fool's mate is Checkmate");
        test::expectEq(r.winner == BLACK, true, "Black delivered mate");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: stalemate]\n";
    {
        ChessGame g;
        g.loadFEN("k7/2K5/1Q6/8/8/8/8/8 b - - 0 1");
        test::expectEq(describe(g.result()), std::string{"Stalemate"}, "classic stalemate position");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: fifty-move]\n";
    {
        ChessGame g;
        g.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 100 51");
        test::expectEq(describe(g.result()), std::string{"FiftyMove"}, "halfmove >= 100 -> FiftyMove");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: insufficient material]\n";
    {
        ChessGame g;
        g.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"InsufficientMaterial"}, "K vs K");
    }
    {
        ChessGame g;
        g.loadFEN("4k3/8/8/8/8/8/8/4KN2 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"InsufficientMaterial"}, "K+N vs K");
    }
    {
        ChessGame g;
        g.loadFEN("4k3/8/8/8/8/8/8/4KB2 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"InsufficientMaterial"}, "K+B vs K");
    }
    {
        // Bishops on f1 and g8. Both are on light squares
        // (f1: file=5, rank=0 -> sum 5 -> odd -> light;
        //  g8: file=6, rank=7 -> sum 13 -> odd -> light).
        // Same color -> insufficient material.
        ChessGame g;
        g.loadFEN("4k1b1/8/8/8/8/8/8/4KB2 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"InsufficientMaterial"},
                       "K+B vs K+B, same color -> InsufficientMaterial");
    }
    {
        // Bishops on f1 and f8. f1 is light, f8 is dark
        // (f1: 5+0 = 5 -> odd -> light;
        //  f8: 5+7 = 12 -> even -> dark).
        // Opposite colors -> sufficient material for a mate.
        ChessGame g;
        g.loadFEN("4kb2/8/8/8/8/8/8/4KB2 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"Ongoing"}, "K+B vs K+B, opposite colors -> Ongoing");
    }

    {
        ChessGame g;
        g.loadFEN("4k3/8/8/8/8/8/8/4KR2 w - - 0 1");
        test::expectEq(describe(g.result()), std::string{"Ongoing"}, "K+R vs K -> Ongoing");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[result: threefold]\n";
    {
        // Shuffle knights back and forth. After 12 plies, the start
        // position's key has appeared at moves_[3], moves_[7], moves_[11].
        ChessGame g;
        play(g, "g1f3");
        play(g, "g8f6");
        play(g, "f3g1");
        play(g, "f6g8");
        play(g, "g1f3");
        play(g, "g8f6");
        play(g, "f3g1");
        play(g, "f6g8");
        play(g, "g1f3");
        play(g, "g8f6");
        play(g, "f3g1");
        play(g, "f6g8");

        test::expectEq(describe(g.result()), std::string{"Threefold"}, "threefold by knight shuffling");
    }

    return test::summary();
}