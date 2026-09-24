#pragma once

#include "BoardAnnotations.hpp"
#include "BoardGeometry.hpp"
#include "ChessGame.hpp"
#include "InputEvent.hpp"
#include "types.h"

#include <array>
#include <cstddef>
#include <optional>

#include <SFML/System/Vector2.hpp>

namespace Gui
{

    struct DragState
    {
        bool active = false;
        Square from = SQ_NONE;
        sf::Vector2f offset{0.0f, 0.0f};
    };

    /// State for a pending promotion choice.
    struct PendingPromotion
    {
        bool active = false;
        Square from = SQ_NONE;
        Square to = SQ_NONE;
        Color side = WHITE;
        std::array<Square, 4> choiceSquares{SQ_NONE, SQ_NONE, SQ_NONE, SQ_NONE};
    };

    /// Result of the most recent controller action.
    struct ActionResult
    {
        enum class Kind
        {
            Ready,
            NotYourTurn,
            Dragging,
            MoveCancelled,
            DroppedOutsideBoard,
            MovePlayed,
            IllegalMove,
            PromotionPending
        };

        Kind kind = Kind::Ready;

        Square from = SQ_NONE;
        Square to = SQ_NONE;
        Piece piece = PIECE_NONE;
    };

    /// Translates input events into moves on a ChessGame.
    class GuiGameController
    {
    public:
        explicit GuiGameController(ChessGame& game)
            : game_(game)
        {
        }

        void handleEvent(const InputEvent& event, const BoardGeometry& geo);
        void cancelDrag();

        const DragState& drag() const
        {
            return drag_;
        }

        const ActionResult& result() const
        {
            return result_;
        }

        const PendingPromotion& pendingPromotion() const
        {
            return promo_;
        }

        const BoardAnnotations& annotations() const
        {
            return annotations_;
        }

        /// True if the most recent move commit originated from a drag
        /// release rather than a click-click. Cleared by navigation
        /// (undo, redo, gotoPly). Consumed by the animation driver.
        bool lastMoveWasDrag() const
        {
            return lastMoveWasDrag_;
        }

        void completePromotion(PieceType type);
        void cancelPromotion();

        void requestUndo();
        void requestRedo();
        void requestGotoPly(std::size_t ply);
        void applyQueuedActions();

        /// Sets the "new game" flag. Application drains it once per frame
        /// via consumeNewGameRequest(). Called by the game-over banner's
        /// New Game button.
        void requestNewGame();

        /// Returns true at most once per requestNewGame(). Clears the flag.
        bool consumeNewGameRequest();

        void updateAnnotations();

    private:
        void onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void onRelease(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void playMoveOrStartPromotion(Square from, Square to, bool fromDrag);

        ChessGame& game_;
        DragState drag_;
        PendingPromotion promo_;
        ActionResult result_{};
        mutable std::optional<std::size_t> queuedGotoPly_;
        BoardAnnotations annotations_;
        mutable bool queuedUndo_ = false;
        mutable bool queuedRedo_ = false;
        bool lastMoveWasDrag_ = false;

        /// Set by requestNewGame(), cleared by consumeNewGameRequest().
        bool newGameRequested_ = false;
    };

} // namespace Gui