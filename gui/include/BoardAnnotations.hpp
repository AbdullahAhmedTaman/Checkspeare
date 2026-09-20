#pragma once

#include "ChessGame.hpp"
#include "types.h"

#include <optional>
#include <vector>

namespace Gui
{

    /// Derived, render-only information about the current position.
    ///
    /// Kept separate from ChessGame so the renderer does not depend on game
    /// logic, and so "what counts as a hint" lives in one place.
    struct BoardAnnotations
    {
        std::optional<Square> lastMoveFrom;
        std::optional<Square> lastMoveTo;

        /// King of the side to move, if that side is currently in check.
        std::optional<Square> checkedKing;

        /// Piece currently selected by the user. Drives legal-target hints.
        std::optional<Square> selected;
        std::vector<Square> legalTargets;

        /// Clears `selected` and `legalTargets`. Called when a drag ends or
        /// the user clicks elsewhere.
        /// @note Not yet implemented.
        void clearSelection();

        /// Recomputes `lastMoveFrom`, `lastMoveTo`, and `checkedKing` from
        /// `game` at its current cursor. Does not touch `selected` or
        /// `legalTargets`, which are driven by GuiGameController.
        /// @note Not yet implemented.
        void refreshFrom(const ChessGame& game);
    };

} // namespace Gui