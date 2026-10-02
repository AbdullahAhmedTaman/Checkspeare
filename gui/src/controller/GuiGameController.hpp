#pragma once

#include "../../../engine/src/board/types.h"
#include "../game/BoardGeometry.hpp"
#include "../game/ChessGame.hpp"
#include "../input/InputEvent.hpp"
#include "../ui/board/BoardAnnotations.hpp"


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

    struct PendingPromotion
    {
        bool active = false;
        Square from = SQ_NONE;
        Square to = SQ_NONE;
        Color side = WHITE;
        std::array<Square, 4> choiceSquares{SQ_NONE, SQ_NONE, SQ_NONE, SQ_NONE};
    };

    struct Arrow
    {
        Square from = SQ_NONE;
        Square to = SQ_NONE;
    };

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

    /// Translates backend-agnostic input events into moves on a ChessGame.
    /// Owns no resources and knows nothing about SFML; see SfmlInputAdapter
    /// for the boundary that produces the InputEvent vocabulary.
    class GuiGameController
    {
    public:
        explicit GuiGameController(ChessGame& game)
            : game_(game)
        {
        }

        void handleEvent(const InputEvent& event, const BoardGeometry& geo);
        void cancelDrag();

        /// Clears `selected` and `legalTargets` if a selection is active.
        /// Called when the user clicks outside the board, so a stale
        /// selection doesn't persist into unrelated UI interaction.
        void cancelSelection();

        void completePromotion(PieceType type);
        void cancelPromotion();

        void requestUndo();
        void requestRedo();
        void requestGotoPly(std::size_t ply);
        void applyQueuedActions();

        void requestNewGame();
        bool consumeNewGameRequest();

        void requestResign(Color side);
        std::optional<Color> consumeResignRequest();

        /// Sets the "quit application" flag. Application drains it once per
        /// frame and closes the window.
        void requestQuit();
        bool consumeQuitRequest();

        bool gameOverModalDismissed() const
        {
            return gameOverModalDismissed_;
        }

        void dismissGameOverModal()
        {
            gameOverModalDismissed_ = true;
        }

        void reopenGameOverModal()
        {
            gameOverModalDismissed_ = false;
        }

        void updateAnnotations();

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

        bool lastMoveWasDrag() const
        {
            return lastMoveWasDrag_;
        }

        const std::vector<Arrow>& arrows() const
        {
            return arrows_;
        }

        void clearArrows()
        {
            arrows_.clear();
            arrowAnchor_.reset();
            rightPressActive_ = false;
        }

    private:
        void onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void onRelease(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void onRightPress(sf::Vector2f position, const BoardGeometry& geo);
        void onRightRelease(sf::Vector2f position, const BoardGeometry& geo);
        void playMoveOrStartPromotion(Square from, Square to, bool fromDrag);
        void onKeyPressed(Key key, bool ctrl);

        /// True if from -> to is a legal promotion in the current position.
        bool isPromotionMove(Square from, Square to) const;
        void addOrReplaceArrow(Square from, Square to);
        bool eraseArrowFrom(Square from);
        ChessGame& game_;

        DragState drag_;
        PendingPromotion promo_;
        ActionResult result_{};
        BoardAnnotations annotations_;
        bool lastMoveWasDrag_ = false;

        std::vector<Arrow> arrows_;
        std::optional<Square> arrowAnchor_;
        bool rightPressActive_ = false;

        mutable std::optional<std::size_t> queuedGotoPly_;
        mutable bool queuedUndo_ = false;
        mutable bool queuedRedo_ = false;
        mutable std::optional<Color> queuedResign_;
        bool newGameRequested_ = false;
        bool gameOverModalDismissed_ = false;
        bool quitRequested_ = false;
    };

} // namespace Gui