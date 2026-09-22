// gui/tests/ChessGameTest.cpp
//
// Standalone unit test for ChessGame's history navigation.
// No SFML. No ImGui. No Application.

#include "ChessGame.hpp"
#include "Board.hpp"
#include "types.h"

#include "attacks.h"
#include "zobrist.h"

#include <iostream>
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

    /// Play a coordinate-pair move like "e2e4". Returns true on success.
    bool play(ChessGame& g, const std::string& fromTo)
    {
        const Square from = parseSquare(fromTo.substr(0, 2));
        const Square to = parseSquare(fromTo.substr(2, 2));
        return g.play(from, to).has_value();
    }

    /// Concatenate the SAN of every record in the given list, space-separated.
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
        g.undo(); // cursor = 2, total = 3

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
        g.gotoStart(); // cursor = 0, total = 3

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
        g.gotoStart(); // cursor = 0, total = 2

        test::expectTrue(!play(g, "e2e5"), "e2e5 illegal returns false");
        test::expectEq(g.cursor(), std::size_t{0}, "cursor still 0");
        test::expectEq(g.totalMoves(), std::size_t{2}, "totalMoves still 2 (tail preserved)");

        // Both redo moves should still be there.
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

        // At cursor 2: e4 and e5 have been played.
        test::expectEq(g.pieceAt(parseSquare("e4")) != PIECE_NONE, true, "e4 occupied at cursor 2");
        test::expectEq(g.pieceAt(parseSquare("e5")) != PIECE_NONE, true, "e5 occupied at cursor 2");

        g.gotoPly(0);

        // At cursor 0: back to the start position.
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

    return test::summary();
}