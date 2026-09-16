#pragma once

#include "BoardGeometry.hpp"
#include "ChessGame.hpp"
#include "InputEvent.hpp"
#include "types.h" // Square, SQ_NONE, Piece, PIECE_NONE

#include <SFML/System/Vector2.hpp>

namespace Gui
{

    /// State of an in-progress drag.
    /// `offset` is the click position relative to the unpadded square top-left.
    struct DragState
    {
        bool active = false;
        Square from = SQ_NONE;
        sf::Vector2f offset{0.0f, 0.0f};
    };

    /// Structured outcome of the most recent controller action.
    /// The controller never builds display strings; consumers (ImGui text,
    /// sound effects, move-log panels) interpret this value themselves.
    struct ActionResult
    {
        enum class Kind
        {
            Ready,               // Initial state, no interaction yet.
            NotYourTurn,         // Pressed an opponent's piece.
            Dragging,            // A friendly piece was picked up.
            MoveCancelled,       // Drop on same square, or drag cancelled.
            DroppedOutsideBoard, // Release landed off-board.
            MovePlayed,          // Legal move applied.
            IllegalMove          // from -> to was rejected by the engine.
        };

        Kind kind = Kind::Ready;

        // Populated for Dragging / MovePlayed / IllegalMove.
        Square from = SQ_NONE;
        Square to = SQ_NONE;

        // Populated for Dragging (the piece being dragged).
        Piece piece = PIECE_NONE;
    };

    /// Translates backend-agnostic input events into engine moves.
    /// Owns no resources; it references a ChessGame supplied at construction.
    /// Knows nothing about SFML -- see SfmlInputAdapter.hpp for the boundary
    /// that turns sf::Event into the InputEvent this class consumes.
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

    private:
        void onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);
        void onRelease(MouseButton button, sf::Vector2f position, const BoardGeometry& geo);

        ChessGame& game_;
        DragState drag_;
        ActionResult result_{};
    };

} // namespace Gui