#include "SetupFlow.hpp"

#include "Config.hpp"
#include "controller/GuiGameController.hpp"
#include "engine/EngineRig.hpp"
#include "game/BoardGeometry.hpp"
#include "game/ChessClock.hpp"
#include "game/ChessGame.hpp"

SetupFlow::SetupFlow(Config& conf, ChessGame& game, ChessClock& clock, EngineRig& engines, Gui::GuiGameController& controller,
                     Gui::BoardGeometry& geo)
    : conf_(conf),
      game_(game),
      clock_(clock),
      engines_(engines),
      controller_(controller),
      geo_(geo)
{
}

void SetupFlow::seedFromConfig()
{
    // Committed values are written back into Config by apply(), so this
    // seed gives the next session the last-used settings.
    pendingSetup_.timeControl = conf_.timeControl;

    // Guard against a stored zero from an earlier session: a clock with
    // zero time falls on the first tick, before either side can move.
    if (pendingSetup_.timeControl.initialMs < 60 * 1000)
        pendingSetup_.timeControl.initialMs = 60 * 1000;

    pendingSetup_.whiteKind = Gui::PlayerKind::Human;
    pendingSetup_.blackKind = Gui::PlayerKind::Engine;
    pendingSetup_.whiteEnginePath = conf_.whiteEnginePath;
    pendingSetup_.blackEnginePath = conf_.blackEnginePath;
}

bool SetupFlow::render()
{
    if (auto committed = Gui::renderGameSetupModal(pendingSetup_, open_))
    {
        apply(*committed);
        open_ = false;
        return true;
    }
    return false;
}

void SetupFlow::apply(const Gui::GameSetup& setup)
{
    conf_.timeControl = setup.timeControl;
    conf_.whiteEnginePath = setup.whiteEnginePath;
    conf_.blackEnginePath = setup.blackEnginePath;

    engines_.abortAll();
    engines_.stopAll();

    game_.reset();

    const bool whiteIsHuman = (setup.whiteKind == Gui::PlayerKind::Human);
    const bool blackIsHuman = (setup.blackKind == Gui::PlayerKind::Human);

    if (whiteIsHuman != blackIsHuman)
        geo_.flipped = !whiteIsHuman;
    else
        geo_.flipped = false;

    // Let Application reset its per-frame bookkeeping (cursor, game-over
    // latch) before the clock and engines come back online.
    if (onApplied_)
        onApplied_();

    // Clamp the initial time to one minute: zero would make the flag fall
    // on the first tick, before either side can move.
    ChessClock::TimeControl tc = setup.timeControl;
    if (tc.initialMs < 60 * 1000)
        tc.initialMs = 60 * 1000;

    clock_.configure(tc);
    clock_.start();
    game_.setInitialClockSnapshot(clock_.snapshot());

    engines_.setSideControlled(WHITE, setup.whiteKind == Gui::PlayerKind::Engine);
    engines_.setSideControlled(BLACK, setup.blackKind == Gui::PlayerKind::Engine);

    engines_.launchOrStop(WHITE, setup.whiteKind == Gui::PlayerKind::Engine, setup.whiteEnginePath);
    engines_.launchOrStop(BLACK, setup.blackKind == Gui::PlayerKind::Engine, setup.blackEnginePath);

    controller_.updateAnnotations();
    controller_.reopenGameOverModal();
    controller_.clearArrows();
}