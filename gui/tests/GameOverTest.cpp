// gui/tests/GameOverTest.cpp
//
// Standalone unit test for the game-over reaction logic.

#include "../src/game/Board.hpp"
#include "../src/game/ChessClock.hpp"
#include "../src/game/ChessGame.hpp"
#include "../src/engine/EnginePlayer.hpp"
#include "../src/game/GameOverTransition.hpp"
#include "../src/game/GameResult.hpp"
#include "../src/controller/GuiGameController.hpp"
#include "../src/input/InputEvent.hpp"

#include "../../engine/src/board/attacks.h"
#include "../../engine/src/board/zobrist.h"

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
            std::cout << "  [PASS] " << label << "\n";
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
            std::cout << "  [PASS] " << label << "\n";
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

    Gui::BoardGeometry makeTestGeo()
    {
        Gui::BoardGeometry geo;
        geo.squareSize = 80.0f;
        geo.boardOrigin = {0.0f, 0.0f};
        geo.flipped = false;
        return geo;
    }

    sf::Vector2f centerOf(Square sq, const Gui::BoardGeometry& geo)
    {
        const sf::Vector2f tl = geo.topLeft(sq);
        return {tl.x + geo.squareSize * 0.5f, tl.y + geo.squareSize * 0.5f};
    }

    void press(Gui::GuiGameController& c, Square sq, const Gui::BoardGeometry& geo)
    {
        const Gui::InputEvent e = Gui::InputEvent::makePointerPressed(Gui::MouseButton::Left, centerOf(sq, geo));
        c.handleEvent(e, geo);
    }

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
        test::expectTrue(c.consumeNewGameRequest(), "first consume true");
        test::expectTrue(!c.consumeNewGameRequest(), "second consume false");

        c.requestNewGame();
        c.requestNewGame();
        c.requestNewGame();
        test::expectTrue(c.consumeNewGameRequest(), "three requests collapse: first true");
        test::expectTrue(!c.consumeNewGameRequest(), "three requests collapse: second false");
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
        test::expectTrue(!c.drag().active, "no drag on frozen game");

        Gui::GuiGameController c2(g);
        press(c2, parseSquare("h4"), geo);
        test::expectTrue(!c2.drag().active, "no drag on opponent's piece");
        test::expectTrue(c2.result().kind == Gui::ActionResult::Kind::Ready, "result stays Ready");
    }
    {
        ChessGame g;
        const Gui::BoardGeometry geo = makeTestGeo();
        Gui::GuiGameController c(g);
        press(c, parseSquare("e2"), geo);
        test::expectTrue(c.drag().active, "live game DOES start a drag");
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

        test::expectEq(g.cursor(), std::size_t{4}, "cursor unchanged");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[drag that started live, released after game over]\n";
    {
        // Start a drag on a live game, then deliver a mate, then release.
        // The release must not play a move.
        ChessGame g;
        const Gui::BoardGeometry geo = makeTestGeo();
        Gui::GuiGameController c(g);

        // Begin a drag on the e2 pawn.
        press(c, parseSquare("e2"), geo);
        test::expectTrue(c.drag().active, "drag starts on live game");

        // Force the game to a terminal position by loading a mated FEN.
        // The controller still holds drag state from the previous frame.
        g.loadFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");

        // Release over e4. Should be dropped, not played.
        release(c, parseSquare("e4"), geo);
        test::expectEq(g.cursor(), std::size_t{0}, "release after game over: no move played");
        test::expectTrue(!c.drag().active, "drag cleared");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[applyQueuedActions drops requests when frozen]\n";
    {
        ChessGame g;
        play(g, "f2f3");
        play(g, "e7e5");
        play(g, "g2g4");
        play(g, "d8h4");

        Gui::GuiGameController c(g);

        // Queue an undo and a goto; both should be dropped.
        c.requestUndo();
        c.requestGotoPly(1);
        const std::size_t before = g.cursor();
        c.applyQueuedActions();

        test::expectEq(g.cursor(), before, "queued undo/goto dropped when frozen");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[justEnteredGameOver transition detector]\n";
    {
        bool wasOver = false;

        test::expectTrue(!justEnteredGameOver(false, wasOver), "1: Ongoing no enter");
        test::expectTrue(!wasOver, "wasOver stays false");
        test::expectTrue(!justEnteredGameOver(false, wasOver), "2: Ongoing no enter");
        test::expectTrue(justEnteredGameOver(true, wasOver), "3: enter fires");
        test::expectTrue(wasOver, "wasOver now true");
        test::expectTrue(!justEnteredGameOver(true, wasOver), "4: no re-fire");
        test::expectTrue(!justEnteredGameOver(true, wasOver), "5: no re-fire");
        test::expectTrue(!justEnteredGameOver(false, wasOver), "6: back to ongoing");
        test::expectTrue(!wasOver, "wasOver reset");
        test::expectTrue(justEnteredGameOver(true, wasOver), "7: fires again");
        test::expectTrue(wasOver, "wasOver true again");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[newGame reset clears game over]\n";
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
        test::expectTrue(g.result().kind == GameResultKind::Ongoing, "after reset: Ongoing");
        test::expectTrue(g.sideToMove() == WHITE, "after reset: White to move");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[game-over modal dismissal]\n";
    {
        ChessGame g;
        Gui::GuiGameController c(g);

        test::expectTrue(!c.gameOverModalDismissed(), "fresh controller: modal not dismissed");

        c.dismissGameOverModal();
        test::expectTrue(c.gameOverModalDismissed(), "after dismiss: dismissed true");

        c.reopenGameOverModal();
        test::expectTrue(!c.gameOverModalDismissed(), "after reopen: dismissed false");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[ChessClock: no increment after flag fall]\n";
    {
        ChessClock clock;
        ChessClock::TimeControl tc;
        tc.initialMs = 1000;
        tc.incrementMs = 500;
        clock.configure(tc);
        clock.start();

        // Push white to flag fall.
        clock.update(2.0);
        test::expectTrue(clock.flagFallen(WHITE), "white flag fell");
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(WHITE)), std::size_t{0}, "white at 0");

        // Try to grant an increment to white.
        clock.onMovePlayed(WHITE);
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(WHITE)), std::size_t{0}, "no increment after flag fall");
    }
    {
        ChessClock clock;
        ChessClock::TimeControl tc;
        tc.initialMs = 5000;
        tc.incrementMs = 500;
        clock.configure(tc);
        clock.start();

        // Live game, increment applies normally.
        clock.onMovePlayed(WHITE);
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(WHITE)), std::size_t{5500}, "increment applies when live");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[EnginePlayer::decideEngineAction respects gameOver]\n";
    {
        EnginePlayerInputs in;
        in.thinking = false;
        in.sideControlled = true;
        in.engineReady = true;
        in.engineAlive = true;
        in.gameOver = true;
        test::expectTrue(decideEngineAction(in) == EnginePlayerAction::Idle, "no search started when gameOver");
    }
    {
        EnginePlayerInputs in;
        in.thinking = false;
        in.sideControlled = true;
        in.engineReady = true;
        in.engineAlive = true;
        in.gameOver = false;
        test::expectTrue(decideEngineAction(in) == EnginePlayerAction::StartSearch, "search starts when game is live");
    }
    {
        EnginePlayerInputs in;
        in.thinking = true;
        in.gameOver = true;
        test::expectTrue(decideEngineAction(in) == EnginePlayerAction::AbortSearch, "in-flight search aborts on gameOver");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[toString mapping]\n";
    {
        GameResult r;
        test::expectTrue(toString(r).empty(), "Ongoing -> empty");

        r = {GameResultKind::Checkmate, WHITE};
        test::expectTrue(toString(r) == "Checkmate - White wins", "Checkmate White");
        r = {GameResultKind::Checkmate, BLACK};
        test::expectTrue(toString(r) == "Checkmate - Black wins", "Checkmate Black");

        r = {GameResultKind::Stalemate, WHITE};
        test::expectTrue(toString(r) == "Stalemate - Draw", "Stalemate");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[clock snapshot round-trip]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 30'000;
        tc.incrementMs = 2'000;
        c.configure(tc);
        c.start();

        c.update(1.0); // white: 30000 -> 29000
        const auto snapA = c.snapshot();

        c.onMovePlayed(WHITE); // white +2000, active = BLACK
        c.update(2.0);         // black: 30000 -> 28000
        const auto snapB = c.snapshot();

        // Restore to snapA: white should be 29000, active WHITE.
        c.restore(snapA);
        test::expectEq(static_cast<std::size_t>(c.remainingMs(WHITE)), std::size_t{29'000}, "restore A: white 29000");
        test::expectEq(static_cast<std::size_t>(c.remainingMs(BLACK)), std::size_t{30'000}, "restore A: black 30000");

        // Restore to snapB: white should be 31000 (29000 + 2000), black 28000.
        c.restore(snapB);
        test::expectEq(static_cast<std::size_t>(c.remainingMs(WHITE)), std::size_t{31'000}, "restore B: white 31000");
        test::expectEq(static_cast<std::size_t>(c.remainingMs(BLACK)), std::size_t{28'000}, "restore B: black 28000");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[chess game restores clock on undo]\n";
    {
        ChessClock clock;
        ChessClock::TimeControl tc;
        tc.initialMs = 30'000;
        tc.incrementMs = 0;
        clock.configure(tc);
        clock.start();

        ChessGame g;
        g.setClockSync(
            [&clock](const ClockSnapshot& s)
            {
                clock.restore(s);
            });
        g.setInitialClockSnapshot(clock.snapshot());

        // Play e2e4 with the clock at 30000/30000.
        play(g, "e2e4");
        clock.onMovePlayed(WHITE);
        g.setClockSnapshotAt(g.cursor() - 1, clock.snapshot());

        // Now white is 30000, black is 30000, active BLACK.
        const std::int64_t whiteAfter1 = clock.remainingMs(WHITE);
        const std::int64_t blackAfter1 = clock.remainingMs(BLACK);

        // Undo. Clock should snap back to the initial snapshot:
        // active WHITE, both sides 30000.
        g.undo();
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(WHITE)), std::size_t{30'000}, "undo: white back to initial");
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(BLACK)), std::size_t{30'000}, "undo: black back to initial");

        // Redo. Clock should snap forward to after-1 snapshot.
        g.redo();
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(WHITE)), static_cast<std::size_t>(whiteAfter1),
                       "redo: white forward");
        test::expectEq(static_cast<std::size_t>(clock.remainingMs(BLACK)), static_cast<std::size_t>(blackAfter1),
                       "redo: black forward");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[quit request flag]\n";
    {
        ChessGame g;
        Gui::GuiGameController c(g);

        test::expectTrue(!c.consumeQuitRequest(), "fresh: no quit request");

        c.requestQuit();
        test::expectTrue(c.consumeQuitRequest(), "first consume true");
        test::expectTrue(!c.consumeQuitRequest(), "second consume false");

        c.requestQuit();
        c.requestQuit();
        test::expectTrue(c.consumeQuitRequest(), "multiple collapse to one");
        test::expectTrue(!c.consumeQuitRequest(), "no second consume");
    }

    return test::summary();
}