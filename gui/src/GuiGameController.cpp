#include "GuiGameController.hpp"
#include <algorithm>

namespace Gui
{

    namespace
    {
        constexpr PieceType kPromotionOrder[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
    } // namespace

    void GuiGameController::handleEvent(const InputEvent& event, const BoardGeometry& geo)
    {
        switch (event.kind)
        {
            case InputEvent::Kind::FocusLost:
                if (promo_.active)
                    cancelPromotion();
                else
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
            annotations_.clearSelection();
            annotations_.refreshFrom(game_);
        }
    }

    void GuiGameController::onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo)
    {
        if (button != MouseButton::Left)
            return;
        if (promo_.active)
        {
            const auto sqOpt = geo.squareAt(position);

            if (!sqOpt)
            {
                cancelPromotion();
                return;
            }

            for (int i = 0; i < 4; ++i)
            {
                if (promo_.choiceSquares[i] == *sqOpt)
                {
                    completePromotion(kPromotionOrder[i]);
                    return;
                }
            }

            cancelPromotion();
            return;
        }
        if (drag_.active)
            return;

        const auto sqOpt = geo.squareAt(position);
        if (!sqOpt)
            return;

        const Square sq = *sqOpt;
        if (annotations_.selected.has_value())
        {
            const auto& targets = annotations_.legalTargets;

            if (std::find(targets.begin(), targets.end(), sq) != targets.end())
            {
                const Square from = *annotations_.selected;
                playMoveOrStartPromotion(from, sq);
                return;
            }
        }

        const Piece pc = game_.pieceAt(sq);

        if (pc == PIECE_NONE)
        {
            annotations_.clearSelection();
            annotations_.refreshFrom(game_);
            return;
        }

        if (color_of(pc) != game_.sideToMove())
        {
            result_ = {ActionResult::Kind::NotYourTurn};
            annotations_.clearSelection();
            annotations_.refreshFrom(game_);
            return;
        }

        annotations_.selected = sq;
        annotations_.legalTargets = game_.board().legalDestinations(sq);
        annotations_.refreshFrom(game_);

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
            annotations_.clearSelection();
            annotations_.refreshFrom(game_);
            return;
        }

        const Square from = drag_.from;
        const Square to = *targetOpt;

        if (from == to)
        {
            drag_ = {};
            result_ = {ActionResult::Kind::MoveCancelled};
            return;
        }

        playMoveOrStartPromotion(from, to);
        drag_ = {};
    }

    void GuiGameController::playMoveOrStartPromotion(Square from, Square to)
    {
        const Piece moving = game_.pieceAt(from);
        bool isPromotion = false;

        if (moving != PIECE_NONE)
        {
            for (const Move& m : game_.board().legalMovesFrom(from))
            {
                if (m.to() == to && m.is_promotion())
                {
                    isPromotion = true;
                    break;
                }
            }
        }

        if (isPromotion)
        {
            const int file = static_cast<int>(to) % 8;
            const int rank = static_cast<int>(to) / 8;
            const bool drawUpward = (rank <= 3);

            promo_.active = true;
            promo_.from = from;
            promo_.to = to;
            promo_.side = game_.sideToMove();

            for (int i = 0; i < 4; ++i)
            {
                const int deltaRank = drawUpward ? (i + 1) : -(i + 1);
                const int r = rank + deltaRank;
                promo_.choiceSquares[i] = (r < 0 || r > 7) ? SQ_NONE : static_cast<Square>(r * 8 + file);
            }

            result_ = {ActionResult::Kind::PromotionPending, from, to, moving};
            annotations_.clearSelection();
            annotations_.refreshFrom(game_);
            return;
        }

        if (game_.play(from, to))
            result_ = {ActionResult::Kind::MovePlayed, from, to};
        else
            result_ = {ActionResult::Kind::IllegalMove, from, to};

        annotations_.clearSelection();
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::completePromotion(PieceType type)
    {
        if (!promo_.active)
            return;

        const Square from = promo_.from;
        const Square to = promo_.to;

        promo_ = {};

        if (game_.play(from, to, type))
            result_ = {ActionResult::Kind::MovePlayed, from, to};
        else
            result_ = {ActionResult::Kind::IllegalMove, from, to};

        annotations_.clearSelection();
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::cancelPromotion()
    {
        if (!promo_.active)
            return;

        const Square from = promo_.from;
        const Square to = promo_.to;

        promo_ = {};
        result_ = {ActionResult::Kind::MoveCancelled, from, to};

        annotations_.clearSelection();
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::applyQueuedActions()
    {
        const bool hadQueued = queuedGotoPly_.has_value() || queuedUndo_ || queuedRedo_;

        if (!hadQueued)
            return;

        if (queuedGotoPly_)
        {
            game_.gotoPly(*queuedGotoPly_);
            queuedGotoPly_.reset();
        }
        else if (queuedUndo_)
        {
            game_.undo();
        }
        else if (queuedRedo_)
        {
            game_.redo();
        }

        queuedUndo_ = false;
        queuedRedo_ = false;

        drag_ = {};
        promo_ = {};
        annotations_.clearSelection();
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::updateAnnotations()
    {
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::requestUndo()
    {
        queuedUndo_ = true;
    }

    void GuiGameController::requestRedo()
    {
        queuedRedo_ = true;
    }

    void GuiGameController::requestGotoPly(std::size_t ply)
    {
        queuedGotoPly_ = ply;
    }

} // namespace Gui