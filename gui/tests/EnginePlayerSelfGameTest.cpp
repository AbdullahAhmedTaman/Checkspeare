// Self-play and stale-search tests for EnginePlayer.
//
// Usage: engine_player_self_game <path-to-uci-engine>

#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"
#include "engine/EnginePlayer.hpp"
#include "engine/UciEngine.hpp"
#include "board/attacks.h"
#include "board/zobrist.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    constexpr int kTargetPlies = 20;
    constexpr auto kOverallTimeout = std::chrono::seconds(30);
    constexpr auto kTickInterval = std::chrono::milliseconds(10);
    constexpr int kUndoAtPly = 5;

    bool runSelfGame(const std::string& enginePath)
    {
        UciEngine engine;
        engine.start(enginePath);

        ChessGame game;
        ChessClock clock;
        // 1 second per side. This is a correctness smoke test, not a strength
        // test: the point is to verify that legal moves come back and get played,
        // not to play a real game. 1s keeps the test under a few seconds.
        clock.configure({.initialMs = 1000, .incrementMs = 0});
        clock.start();

        EnginePlayer player(engine);
        player.setSideControlled(WHITE, true);
        player.setSideControlled(BLACK, true);

        const auto deadline = std::chrono::steady_clock::now() + kOverallTimeout;
        bool didUndo = false;
        bool lastTickPlayedMove = true; // start true so the first tick can start a search

        while (game.cursor() < kTargetPlies && std::chrono::steady_clock::now() < deadline)
        {
            if (auto m = player.tick(game, clock))
            {
                const auto ply = game.cursor();
                const auto& san = game.history().back().san;

                std::cout << "ply " << ply << "  " << san << '\n';
                lastTickPlayedMove = true;

                // Force a stale search once: undo immediately after the
                // move. The player is now thinking about a position the
                // game has left behind. The next tick must abort cleanly
                // and restart for the current position.
                if (!didUndo && lastTickPlayedMove && game.cursor() == kUndoAtPly)
                {
                    didUndo = true;
                    std::cout << "[test] undoing while search is in flight\n";
                    game.undo();
                }
                lastTickPlayedMove = false;
            }

            std::this_thread::sleep_for(kTickInterval);
        }

        engine.stop();

        if (static_cast<int>(game.cursor()) < kTargetPlies)
        {
            std::cerr << "FAIL: only reached ply " << game.cursor() << " (target " << kTargetPlies << ")\n";
            return false;
        }

        if (!didUndo)
        {
            std::cerr << "FAIL: undo-at-ply " << kUndoAtPly << " never triggered; increase kTargetPlies\n";
            return false;
        }

        std::cout << "self-game OK: reached " << game.cursor() << " plies, undo exercised\n";
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    Zobrist::init();
    init_attacks();
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <path-to-uci-engine>\n";
        return EXIT_FAILURE;
    }

    try
    {
        return runSelfGame(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::cerr << "exception: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}