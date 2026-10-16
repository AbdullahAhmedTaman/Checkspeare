// gui/tests/SanTest.cpp
//
// Standalone unit test for San.cpp. No SFML. No ImGui. No ChessGame.

#include "game/San.hpp"
#include "game/Board.hpp"

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

    std::optional<Move> findMove(const Board& board, Square from, Square to, PieceType promo = QUEEN)
    {
        for (const Move& m : board.allLegalMoves())
        {
            if (m.from() != from || m.to() != to)
                continue;
            if (m.is_promotion() && promotion_type(m) != promo)
                continue;
            return m;
        }
        return std::nullopt;
    }

    std::string sanOf(const Board& board, const std::string& fromTo, PieceType promo = QUEEN)
    {
        const Square from = parseSquare(fromTo.substr(0, 2));
        const Square to = parseSquare(fromTo.substr(2, 2));
        const auto m = findMove(board, from, to, promo);
        if (!m)
            return "[illegal]";
        return toSan(board, *m);
    }

} // namespace

int main()
{
    init_attacks();
    Zobrist::init();

    // ---------- Pawn moves ----------
    std::cout << "\n[Pawn moves]\n";
    {
        Board b;
        test::expectEq(sanOf(b, "e2e4"), "e4", "e2e4 -> e4");
        test::expectEq(sanOf(b, "d2d4"), "d4", "d2d4 -> d4");
        test::expectEq(sanOf(b, "a2a3"), "a3", "a2a3 -> a3");
    }

    // ---------- Piece moves ----------
    std::cout << "\n[Piece moves]\n";
    {
        Board b;
        test::expectEq(sanOf(b, "g1f3"), "Nf3", "g1f3 -> Nf3");
        test::expectEq(sanOf(b, "b1c3"), "Nc3", "b1c3 -> Nc3");
    }
    {
        Board b;
        b.tryMove(parseSquare("e2"), parseSquare("e4"));
        b.tryMove(parseSquare("e7"), parseSquare("e5")); // Black moves, White to move
        test::expectEq(sanOf(b, "f1c4"), "Bc4", "f1c4 -> Bc4");
    }

    // ---------- Captures ----------
    std::cout << "\n[Captures]\n";
    {
        Board b;
        b.loadFEN("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "e4d5"), "exd5", "e4xd5 -> exd5");
    }
    {
        Board b;
        b.loadFEN("4k3/8/8/8/8/8/8/R3K2R w - - 0 1");
        // rook takes nothing on a8 in this position; use a capture instead
        b.loadFEN("r3k3/8/8/8/8/8/8/R3K3 w - - 0 1");
        test::expectEq(sanOf(b, "a1a8"), "Rxa8+", "Ra1xa8+ rook capture w/ check");
    }

    // ---------- Castling ----------
    std::cout << "\n[Castling]\n";
    {
        Board b;
        b.tryMove(parseSquare("e2"), parseSquare("e4"));
        b.tryMove(parseSquare("e7"), parseSquare("e5"));
        b.tryMove(parseSquare("g1"), parseSquare("f3"));
        b.tryMove(parseSquare("b8"), parseSquare("c6"));
        b.tryMove(parseSquare("f1"), parseSquare("c4"));
        b.tryMove(parseSquare("f8"), parseSquare("c5"));
        test::expectEq(sanOf(b, "e1g1"), "O-O", "e1g1 -> O-O");
    }
    {
        Board b;
        b.tryMove(parseSquare("d2"), parseSquare("d4"));
        b.tryMove(parseSquare("d7"), parseSquare("d5"));
        b.tryMove(parseSquare("b1"), parseSquare("c3"));
        b.tryMove(parseSquare("b8"), parseSquare("c6"));
        b.tryMove(parseSquare("c1"), parseSquare("f4"));
        b.tryMove(parseSquare("c8"), parseSquare("f5"));
        b.tryMove(parseSquare("d1"), parseSquare("d2"));
        b.tryMove(parseSquare("d8"), parseSquare("d7"));
        test::expectEq(sanOf(b, "e1c1"), "O-O-O", "e1c1 -> O-O-O");
    }

    // ---------- Promotion ----------
    std::cout << "\n[Promotion]\n";
    {
        Board b;
        // Black king on h8 (safe from the e7 pawn, which attacks d8/f8).
        // After promotion, Q/R on e8 attack along rank 8 -> check on h8.
        // B/N on e8 do NOT attack h8 -> no check.
        b.loadFEN("7k/4P3/8/8/8/8/8/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "e7e8", QUEEN), "e8=Q+", "promotion Q -> e8=Q+");
        test::expectEq(sanOf(b, "e7e8", ROOK), "e8=R+", "promotion R -> e8=R+");
        test::expectEq(sanOf(b, "e7e8", BISHOP), "e8=B", "promotion B -> e8=B");
        test::expectEq(sanOf(b, "e7e8", KNIGHT), "e8=N", "promotion N -> e8=N");
    }
    {
        Board b;
        b.loadFEN("3rk3/4P3/8/8/8/8/8/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "e7d8", QUEEN), "exd8=Q+", "exd8=Q+ capture-promotion");
    }

    // ---------- Check / mate ----------
    std::cout << "\n[Check and checkmate]\n";
    {
        Board b;
        b.tryMove(parseSquare("f2"), parseSquare("f3"));
        b.tryMove(parseSquare("e7"), parseSquare("e5"));
        b.tryMove(parseSquare("g2"), parseSquare("g4"));
        test::expectEq(sanOf(b, "d8h4"), "Qh4#", "Qh4 mate");
    }
    {
        Board b;
        b.loadFEN("4k3/8/8/8/7Q/8/8/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "h4e7"), "Qe7+", "Qe7+ check");
    }
    {
        Board b;
        b.loadFEN("4k3/5N2/8/8/8/8/8/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "f7d6"), "Nd6+", "Nd6+ knight check");
    }

    // ---------- Disambiguation: file ----------
    std::cout << "\n[Disambiguation - file]\n";
    {
        Board b;
        b.loadFEN("4k3/8/8/8/8/8/3N4/4K1N1 w - - 0 1");
        test::expectEq(sanOf(b, "g1f3"), "Ngf3", "Ngf3 (file disambig)");
        test::expectEq(sanOf(b, "d2f3"), "Ndf3", "Ndf3 (file disambig)");
    }

    // ---------- Disambiguation: rank ----------
    std::cout << "\n[Disambiguation - rank]\n";
    {
        Board b;
        // Knights on d2 and d4, both on the d-file, both reach f3.
        // Rank alone disambiguates: N2f3 vs N4f3.
        b.loadFEN("4k3/8/8/8/3N4/8/3N4/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "d2f3"), "N2f3", "N2f3 (rank disambig)");
        test::expectEq(sanOf(b, "d4f3"), "N4f3", "N4f3 (rank disambig)");
    }

    // ---------- Disambiguation: full square ----------
    std::cout << "\n[Disambiguation - full square]\n";
    {
        Board b;
        // Knights on d2, d4, h2. For mover d2 -> f3:
        //   d4 shares file 'd' (fileCollides)
        //   h2 shares rank '2' (rankCollides)
        // So neither alone suffices -> full square, "Nd2f3".
        b.loadFEN("4k3/8/8/8/3N4/8/3N3N/4K3 w - - 0 1");
        test::expectEq(sanOf(b, "d2f3"), "Nd2f3", "Nd2f3 (full-square disambig)");
    }

    // ---------- Disambiguation: capture + file ----------
    std::cout << "\n[Disambiguation - capture + file]\n";
    {
        Board b;
        b.loadFEN("4k3/8/8/8/8/5p2/3N4/4K1N1 w - - 0 1");
        test::expectEq(sanOf(b, "g1f3"), "Ngxf3", "Ngxf3 (disambig + capture)");
    }

    // ---------- Illegal moves ----------
    std::cout << "\n[Illegal moves]\n";
    {
        Board b;
        test::expectEq(sanOf(b, "e2e5"), "[illegal]", "e2e5 illegal");
        test::expectEq(sanOf(b, "g1e4"), "[illegal]", "g1e4 illegal");
    }

    return test::summary();
}