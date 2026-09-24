// gui/tests/GameOverTest.cpp
//
// Standalone unit test for the game-over reaction logic:
//   - ChessGame::result() terminal states
//   - GuiGameController::requestNewGame / consumeNewGameRequest flag queue
//   - GuiGameController::onPress freeze when result() is terminal
//   - justEnteredGameOver transition detector
//   - ChessGame::reset() clears the game-over state
//
// No SFML, no ImGui, no Application. The controller is driven with synthetic
// InputEvents; geometry is a fixed 8x8 board at a known pixel size.
#include "Board.hpp"
#include "ChessGame.hpp"
#include "GameOverTransition.hpp"
#include "GameResult.hpp"
#include "GuiGameController.hpp"
#include "InputEvent.hpp"

#include "attacks.h"
#include "zobrist.h"

#include <iostream>
#include <string>

// ============================================================================
//  Minimal test framework
// ============================================================================
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

    int summary()
    {
        std::cout << "\n=== " << (g_checks - g_failures) << "/" << g_checks << " checks passed ===\n";
        return g_failures == 0 ? 0 : 1;
    }
} // namespace test

// ============================================================================
//  Helpers
// ============================================================================
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

    /// A BoardGeometry sized for test use. 8 x 80px = 640px, anchored at (0,0).
    Gui::BoardGeometry makeTestGeo()
    {
        Gui::BoardGeometry geo;
        geo.squareSize = 80.0f;
        geo.boardOrigin = {0.0f, 0.0f};
        geo.flipped = false;
        return geo;
    }

    /// Pixel centre of `sq` under the test geometry.
    sf::Vector2f centerOf(Square sq, const Gui::BoardGeometry& geo)
    {
        const sf::Vector2f tl = geo.topLeft(sq);
        return {tl.x + geo.squareSize * 0.5f, tl.y + geo.squareSize * 0.5f};
    }

    /// Simulate a press on `sq`.
    void press(Gui::GuiGameController& c, Square sq, const Gui::BoardGeometry& geo)
    {
        const Gui::InputEvent e = Gui::InputEvent::makePointerPressed(Gui::MouseButton::Left, centerOf(sq, geo));
        c.handleEvent(e, geo);
    }

    /// Simulate a release on `sq`.
    void release(Gui::GuiGameController& c, Square sq, const Gui::BoardGeometry& geo)
    {
        const Gui::InputEvent e = Gui::InputEvent::makePointerReleased(Gui::MouseButton::Left, centerOf(sq, geo));
        c.handleEvent(e, geo);
    }

} // namespace

// ============================================================================
//  Tests
// ============================================================================
int main()
{
    init_attacks();
    Zobrist::init();

    // ------------------------------------------------------------------
    std::cout << "\n[result() terminal states]\n";
    {
        ChessGame g;
        test::expectTrue(g.result().kind == GameResultKind::Ongoing, "start position is Ongoing");
    }
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");
        test::expectTrue(g.result().kind == GameResultKind::Checkmate, "Fool's mate -> Checkmate");
        test::expectTrue(g.result().winner == BLACK, "winner is Black");
    }
    {
        ChessGame g;
        g.loadFEN("k7/2K5/1Q6/8/8/8/8/8 b - - 0 1");
        test::expectTrue(g.result().kind == GameResultKind::Stalemate, "stalemate FEN -> Stalemate");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[consumeNewGameRequest flag queue]\n";
    {
        ChessGame g;
        Gui::GuiGameController c(g);

        test::expectTrue(!c.consumeNewGameRequest(), "fresh controller: no request");

        c.requestNewGame();
        test::expectTrue(c.consumeNewGameRequest(), "after requestNewGame: first consume true");
        test::expectTrue(!c.consumeNewGameRequest(), "immediately after: second consume false");

        c.requestNewGame();
        c.requestNewGame();
        c.requestNewGame();
        test::expectTrue(c.consumeNewGameRequest(), "three requests collapse: first consume true");
        test::expectTrue(!c.consumeNewGameRequest(), "three requests collapse: second consume false");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[onPress freeze when result is terminal]\n";
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");

        const Gui::BoardGeometry geo = makeTestGeo();

        Gui::GuiGameController c(g);
        press(c, parseSquare("e1"), geo);
        test::expectTrue(!c.drag().active, "onPress after game over does not start a drag");

        Gui::GuiGameController c2(g);
        press(c2, parseSquare("h4"), geo);
        test::expectTrue(!c2.drag().active, "onPress on opponent after game over does not drag");
        test::expectTrue(c2.result().kind == Gui::ActionResult::Kind::Ready,
                         "onPress on opponent after game over: result stays Ready");
    }
    {
        // Sanity: same press on a live game DOES start a drag.
        ChessGame g;
        const Gui::BoardGeometry geo = makeTestGeo();
        Gui::GuiGameController c(g);
        press(c, parseSquare("e2"), geo);
        test::expectTrue(c.drag().active, "onPress on live game DOES start a drag");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[release after frozen press does nothing]\n";
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");

        const Gui::BoardGeometry geo = makeTestGeo();
        Gui::GuiGameController c(g);

        press(c, parseSquare("e2"), geo);
        release(c, parseSquare("e4"), geo);

        test::expectEq(g.cursor(), std::size_t{4}, "cursor unchanged: still 4 plies");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[justEnteredGameOver transition detector]\n";
    {
        bool wasOver = false;

        test::expectTrue(!justEnteredGameOver(false, wasOver), "frame 1: Ongoing -> no enter");
        test::expectTrue(!wasOver, "wasOver stays false");

        test::expectTrue(!justEnteredGameOver(false, wasOver), "frame 2: Ongoing -> no enter");
        test::expectTrue(!wasOver, "wasOver stays false");

        test::expectTrue(justEnteredGameOver(true, wasOver), "frame 3: enter game over -> fires");
        test::expectTrue(wasOver, "wasOver now true");

        test::expectTrue(!justEnteredGameOver(true, wasOver), "frame 4: still over -> no re-fire");
        test::expectTrue(!justEnteredGameOver(true, wasOver), "frame 5: still over -> no re-fire");

        test::expectTrue(!justEnteredGameOver(false, wasOver), "frame 6: back to ongoing -> no fire");
        test::expectTrue(!wasOver, "wasOver reset to false");

        test::expectTrue(justEnteredGameOver(true, wasOver), "frame 7: enter again -> fires");
        test::expectTrue(wasOver, "wasOver true again");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[newGame simulation: reset clears game over]\n";
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");
        test::expectTrue(g.result().kind == GameResultKind::Checkmate, "before reset: Checkmate");

        g.reset();

        test::expectEq(g.cursor(), std::size_t{0}, "after reset: cursor 0");
        test::expectEq(g.totalMoves(), std::size_t{0}, "after reset: 0 moves");
        test::expectTrue(g.result().kind == GameResultKind::Ongoing, "after reset: Ongoing again");
        test::expectTrue(g.sideToMove() == WHITE, "after reset: White to move");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[newGame flag survives across frame boundaries]\n";
    {
        ChessGame g;
        Gui::GuiGameController c(g);

        c.requestNewGame();

        test::expectTrue(c.consumeNewGameRequest(), "request set on N consumed on N+1");
        test::expectTrue(!c.consumeNewGameRequest(), "and not consumed twice");
    }

    return test::summary();
}