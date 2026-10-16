#pragma once

#include "game/ChessClock.hpp"

#include <optional>
#include <string>

namespace Gui
{

    /// Which kind of opponent plays a side.
    enum class PlayerKind
    {
        Human,
        Engine,
    };

    /// Configuration chosen in the setup modal before a game starts.
    ///
    /// This is pure data: the modal edits it, Application applies it. It
    /// deliberately does not reference any engine, clock, or game object,
    /// so the modal can be rendered without any of them being in scope.
    struct GameSetup
    {
        ChessClock::TimeControl timeControl;

        PlayerKind whiteKind = PlayerKind::Human;
        PlayerKind blackKind = PlayerKind::Engine;

        std::string whiteEnginePath;
        std::string blackEnginePath;
    };

    /// Renders the setup modal.
    ///
    /// `pending` is the edit buffer. Widgets read and write it, but nothing
    /// is committed until the user clicks Start -- at which point the
    /// committed value is returned. The caller owns `pending` and decides
    /// what to do with the result.
    ///
    /// `shouldOpen` requests that the modal be shown. Once shown it stays
    /// open across frames until the user commits; the caller must then
    /// clear `shouldOpen` on its side (the modal itself cannot mutate the
    /// caller's flag).
    std::optional<GameSetup> renderGameSetupModal(GameSetup& pending, bool shouldOpen);

} // namespace Gui