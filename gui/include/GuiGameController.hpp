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

    /// Set while a promotion is pending: the drop has been accepted, but the
    /// piece type is not known yet. While `active`, input is ignored until
    /// completePromotion() or cancelPromotion() is called.
    struct PendingPromotion
    {
        bool active = false;
        Square from = SQ_NONE;
        Square to = SQ_NONE;
        Color side = WHITE;
        std::array<Square, 4> choiceSquares{SQ_NONE, SQ_NONE, SQ_NONE, SQ_NONE};
    };

    /// Structured outcome of the most recent controller action. The controller
    /// never builds display strings; consumers interpret this themselves.
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
    /// Owns no resources and knows nothing about SFML; see SfmlInputAdapter.hpp
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

        void completePromotion(PieceType type);

        void cancelPromotion();

        void queueGotoPly(std::size_t ply) const
        {
            queuedGotoPly_ = ply;
        }

        void applyQueuedActions();

        const BoardAnnotations& annotations() const
        {
            return annotations_;
        }

        void updateAnnotations(const BoardGeometry& geo);

    private:
        void onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void onRelease(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void playMoveOrStartPromotion(Square from, Square to);

        ChessGame& game_;
        DragState drag_;
        PendingPromotion promo_;
        ActionResult result_{};
        mutable std::optional<std::size_t> queuedGotoPly_;
        BoardAnnotations annotations_;
    };

} // namespace Gui