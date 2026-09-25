// gui/tests/ChessClockTest.cpp
//
// Standalone unit test for ChessClock. No SFML, no ImGui.

#include "ChessClock.hpp"
#include "types.h"

#include <iostream>
#include <string>

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

    void expectEq(std::int64_t actual, std::int64_t expected, const std::string& label)
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
                      << "         expected: " << expected << " ms\n"
                      << "         actual:   " << actual << " ms\n";
        }
    }

    int summary()
    {
        std::cout << "\n=== " << (g_checks - g_failures) << "/" << g_checks << " checks passed ===\n";
        return g_failures == 0 ? 0 : 1;
    }
} // namespace test

int main()
{
    // ------------------------------------------------------------------
    std::cout << "\n[configure]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 60'000;  // 1 minute
        tc.incrementMs = 1'000; // 1 second
        c.configure(tc);

        test::expectEq(c.remainingMs(WHITE), 60'000, "white starts at 60000");
        test::expectEq(c.remainingMs(BLACK), 60'000, "black starts at 60000");
        test::expectTrue(!c.running(), "clock is not running after configure");
        test::expectTrue(!c.flagFallen(WHITE), "white not flagged");
        test::expectTrue(!c.flagFallen(BLACK), "black not flagged");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[does not tick until started]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 10'000;
        c.configure(tc);

        c.update(1.0);
        test::expectEq(c.remainingMs(WHITE), 10'000, "no tick before start()");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[ticks the active side]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 10'000;
        c.configure(tc);
        c.start();

        c.update(1.0);
        test::expectEq(c.remainingMs(WHITE), 9'000, "white loses 1000ms after 1s");
        test::expectEq(c.remainingMs(BLACK), 10'000, "black unchanged while white is active");

        c.update(0.5);
        test::expectEq(c.remainingMs(WHITE), 8'500, "white loses 500ms after 0.5s more");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[onMovePlayed flips side and adds increment]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 10'000;
        tc.incrementMs = 2'000;
        c.configure(tc);
        c.start();

        c.update(1.0);         // white: 10000 -> 9000
        c.onMovePlayed(WHITE); // white: 9000 + 2000 = 11000, active = BLACK
        test::expectEq(c.remainingMs(WHITE), 11'000, "white gets +2000 increment after moving");
        test::expectEq(c.remainingMs(BLACK), 10'000, "black unchanged");

        c.update(1.0); // black: 10000 -> 9000
        test::expectEq(c.remainingMs(BLACK), 9'000, "black now loses time");
        test::expectEq(c.remainingMs(WHITE), 11'000, "white unchanged while black is active");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[zero increment]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 5'000;
        tc.incrementMs = 0;
        c.configure(tc);
        c.start();

        c.update(1.0);
        c.onMovePlayed(WHITE);
        test::expectEq(c.remainingMs(WHITE), 4'000, "no increment means time stays lost");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[flag falls at 0]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 2'000;
        c.configure(tc);
        c.start();

        const auto first = c.update(1.0);
        test::expectTrue(!first.has_value(), "1s in, no flag yet");

        const auto second = c.update(1.5);
        test::expectTrue(second.has_value(), "flag event returns a side");
        test::expectTrue(*second == WHITE, "white is the flag-faller");
        test::expectEq(c.remainingMs(WHITE), 0, "white sits at 0");
        test::expectTrue(c.flagFallen(WHITE), "flagFallen(WHITE)");
        test::expectTrue(!c.flagFallen(BLACK), "black not flagged");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[no re-flag after flag fell]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 500;
        c.configure(tc);
        c.start();

        const auto f1 = c.update(1.0);
        test::expectTrue(f1.has_value(), "flag fell on first update");

        const auto f2 = c.update(1.0);
        test::expectTrue(!f2.has_value(), "flag does not re-fire");
        test::expectEq(c.remainingMs(WHITE), 0, "still 0");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[reset restores initial]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 10'000;
        c.configure(tc);
        c.start();
        c.update(2.0);
        c.onMovePlayed(WHITE);

        c.reset();
        test::expectEq(c.remainingMs(WHITE), 10'000, "white back to initial");
        test::expectEq(c.remainingMs(BLACK), 10'000, "black back to initial");
        test::expectTrue(!c.running(), "reset stops the clock");
        test::expectTrue(!c.flagFallen(WHITE), "white flag cleared");
        test::expectTrue(!c.flagFallen(BLACK), "black flag cleared");
    }

    // ------------------------------------------------------------------
    std::cout << "\n[stop halts ticking]\n";
    {
        ChessClock c;
        ChessClock::TimeControl tc;
        tc.initialMs = 10'000;
        c.configure(tc);
        c.start();
        c.update(1.0);
        c.stop();
        c.update(1.0);
        test::expectEq(c.remainingMs(WHITE), 9'000, "time does not decrease after stop()");
    }

    return test::summary();
}