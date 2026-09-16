#include "GuiGameController.hpp"

namespace Gui
{

    void GuiGameController::handleEvent(const InputEvent& event, const BoardGeometry& geo)
    {
        // Window losing focus cancels a drag, otherwise the state can leak.
        switch (event.kind)
        {
            case InputEvent::Kind::FocusLost:
                cancelDrag();
                return;
            case InputEvent::Kind::PointerPressed:
                onPress(event.button, event.position, geo);
                return;
            case InputEvent::Kind::PointerReleased:
                onRelease(event.button, event.position, geo);
                return;
        }
    }

    void GuiGameController::cancelDrag()
    {
        if (drag_.active)
        {
            drag_ = {};
            result_ = {ActionResult::Kind::MoveCancelled};
        }
    }

    void GuiGameController::onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo)
    {
        if (button != MouseButton::Left)
            return;
        if (drag_.active)
            return;

        const auto sqOpt = geo.squareAt(position);
        if (!sqOpt)
            return;

        const Square sq = *sqOpt;
        const Piece pc = game_.pieceAt(sq);
        if (pc == PIECE_NONE)
            return;

        if (color_of(pc) != game_.sideToMove())
        {
            result_ = {ActionResult::Kind::NotYourTurn};
            return;
        }

        drag_.active = true;
        drag_.from = sq;

        const sf::Vector2f tl = geo.topLeft(sq);
        drag_.offset = {position.x - tl.x, position.y - tl.y};

        result_ = {ActionResult::Kind::Dragging, sq, SQ_NONE, pc};
    }

    void GuiGameController::onRelease(MouseButton button, sf::Vector2f position, const BoardGeometry& geo)
    {
        if (button != MouseButton::Left || !drag_.active)
            return;

        const auto targetOpt = geo.squareAt(position);

        if (!targetOpt)
        {
            result_ = {ActionResult::Kind::DroppedOutsideBoard};
            drag_ = {};
            return;
        }

        const Square from = drag_.from;
        const Square to = *targetOpt;

        if (from == to)
        {
            result_ = {ActionResult::Kind::MoveCancelled};
            drag_ = {};
            return;
        }

        if (game_.tryMove(from, to))
            result_ = {ActionResult::Kind::MovePlayed, from, to};
        else
            result_ = {ActionResult::Kind::IllegalMove, from, to};

        drag_ = {};
    }

} // namespace Gui