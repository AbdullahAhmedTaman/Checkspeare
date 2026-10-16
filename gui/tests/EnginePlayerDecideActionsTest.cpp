// tests/decide_engine_action_test.cpp

#include "engine/EnginePlayer.hpp"

#include <cassert>
#include <iostream>

namespace
{
    EnginePlayerInputs base()
    {
        EnginePlayerInputs in;
        in.sideControlled = true;
        in.engineAlive = true;
        in.engineReady = true;
        return in;
    }

    void check(const char* name, const EnginePlayerInputs& in, EnginePlayerAction expected)
    {
        const auto got = decideEngineAction(in);
        if (got != expected)
        {
            std::cerr << "FAIL: " << name << '\n';
            std::exit(1);
        }
    }
} // namespace

int main()
{
    // --- Not thinking ------------------------------------------------
    {
        auto in = base();
        in.sideControlled = false;
        check("not thinking, side not controlled", in, EnginePlayerAction::Idle);
    }
    {
        auto in = base();
        in.engineReady = false;
        check("not thinking, engine not ready", in, EnginePlayerAction::Idle);
    }
    {
        auto in = base();
        in.engineAlive = false;
        check("not thinking, engine not alive", in, EnginePlayerAction::Idle);
    }
    {
        auto in = base();
        check("not thinking, controlled, ready", in, EnginePlayerAction::StartSearch);
    }

    // --- Thinking ----------------------------------------------------
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 10;
        check("thinking, nothing happening", in, EnginePlayerAction::Idle);
    }
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 10;
        in.bestMoveArrived = true;
        check("thinking, bestmove arrived", in, EnginePlayerAction::FinishSearch);
    }
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 8;
        check("thinking, game moved on", in, EnginePlayerAction::AbortSearch);
    }
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 10;
        in.engineAlive = false;
        check("thinking, engine died", in, EnginePlayerAction::AbortSearch);
    }

    // --- The two priority cases -------------------------------------
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 8;         // stale
        in.bestMoveArrived = true; // but a move is in hand
        check("thinking, stale + bestmove -> abort wins", in, EnginePlayerAction::AbortSearch);
    }
    {
        auto in = base();
        in.thinking = true;
        in.thinkStartPly = 10;
        in.gameCursor = 10;
        in.engineAlive = false;    // engine died
        in.bestMoveArrived = true; // but sent its move first
        check("thinking, dead + bestmove -> finish wins", in, EnginePlayerAction::FinishSearch);
    }

    std::cout << "decideEngineAction: all assertions passed\n";
    return 0;
}