#include "GuiGameController.hpp"
#include "../game/GameResult.hpp"

#include <algorithm>

namespace Gui
{

    namespace
    {
        constexpr PieceType kPromotionOrder[4] = {QUEEN, ROOK, BISHOP, KNIGHT};

        bool isFrozen(const ChessGame& game)
        {
            return game.result().kind != GameResultKind::Ongoing;
        }
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
                arrowAnchor_.reset();
                rightPressActive_ = false;
                return;
            case InputEvent::Kind::PointerPressed:
                onPress(event.button, event.position, geo);
                return;
            case InputEvent::Kind::PointerReleased:
                onRelease(event.button, event.position, geo);
                return;
            case InputEvent::Kind::KeyPressed:
                onKeyPressed(event.key, event.ctrl);
                return;
        }
    }

    void GuiGameController::cancelDrag()
    {
        if (drag_.active)
        {
            drag_ = {};
            result_ = {ActionResult::Kind::MoveCancelled};
            cancelSelection();
        }
    }

    void GuiGameController::cancelSelection()
    {
        if (!annotations_.selected.has_value())
            return;

        annotations_.clearSelection();
        annotations_.refreshFrom(game_);
    }

    void GuiGameController::onPress(MouseButton button, sf::Vector2f position, const BoardGeometry& geo)
    {
        if (button == MouseButton::Right)
        {
            onRightPress(position, geo);
            return;
        }
        if (button != MouseButton::Left)
            return;

        if (isFrozen(game_))
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
                playMoveOrStartPromotion(from, sq, /*fromDrag=*/false);
                return;
            }
        }

        const Piece pc = game_.pieceAt(sq);

        if (pc == PIECE_NONE)
        {
            cancelSelection();
            clearArrows();
            return;
        }

        if (color_of(pc) != game_.sideToMove())
        {
            result_ = {ActionResult::Kind::NotYourTurn};
            cancelSelection();
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
        if (button == MouseButton::Right)
        {
            onRightRelease(position, geo);
            return;
        }

        if (button != MouseButton::Left || !drag_.active)
            return;

        if (isFrozen(game_))
        {
            drag_ = {};
            cancelSelection();
            return;
        }

        const auto targetOpt = geo.squareAt(position);

        if (!targetOpt)
        {
            result_ = {ActionResult::Kind::DroppedOutsideBoard};
            drag_ = {};
            cancelSelection();
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

        playMoveOrStartPromotion(from, to, /*fromDrag=*/true);
        drag_ = {};
    }

    void GuiGameController::onRightPress(sf::Vector2f position, const BoardGeometry& geo)
    {
        const auto sqOpt = geo.squareAt(position);

        if (!sqOpt)
        {
            // Right-click outside the board clears every arrow. Matches
            // chess.com and lichess; useful as a single-gesture reset.
            clearArrows();
            return;
        }

        rightPressActive_ = true;

        const Square sq = *sqOpt;

        // A two-click chain is in progress: this click completes it.
        if (arrowAnchor_.has_value())
        {
            const Square from = *arrowAnchor_;
            arrowAnchor_.reset();

            if (from == sq)
            {
                // Same square twice: no arrow.
                return;
            }

            addOrReplaceArrow(from, sq);
            return;
        }

        // An arrow already starts on this square: erase it and consume
        // the click. This is the single-right-click erase gesture.
        if (eraseArrowFrom(sq))
            return;

        // Otherwise, start a new chain. The arrow materializes on either
        // the second click (click-click gesture) or the release (drag).
        arrowAnchor_ = sq;
    }

    void GuiGameController::onRightRelease(sf::Vector2f position, const BoardGeometry& geo)
    {
        if (!rightPressActive_)
            return;

        rightPressActive_ = false;

        // No pending anchor means the press was an erase or a clear;
        // nothing for the release to complete.
        if (!arrowAnchor_.has_value())
            return;

        const auto sqOpt = geo.squareAt(position);
        if (!sqOpt)
            return;

        if (*sqOpt == *arrowAnchor_)
            return; // no drag; wait for the second click

        const Square from = *arrowAnchor_;
        const Square to = *sqOpt;
        arrowAnchor_.reset();

        addOrReplaceArrow(from, to);
    }

    bool GuiGameController::isPromotionMove(Square from, Square to) const
    {
        if (game_.pieceAt(from) == PIECE_NONE)
            return false;

        for (const Move& m : game_.board().legalMovesFrom(from))
        {
            if (m.to() == to && m.is_promotion())
                return true;
        }

        return false;
    }

    void GuiGameController::onKeyPressed(Key key, bool ctrl)
    {
        // Modals and a finished game own the keyboard; nothing the user
        // presses should change the cursor in either state.
        if (isFrozen(game_))
            return;

        switch (key)
        {
            case Key::Z:
                if (ctrl)
                    requestUndo();
                break;

            case Key::X:
                if (ctrl)
                    requestRedo();
                break;

            case Key::Y:
                if (ctrl)
                    requestRedo();
                break;

            case Key::Left:
                if (ctrl)
                    game_.gotoStart();
                else
                    game_.undo();
                break;

            case Key::Right:
                if (ctrl)
                    game_.gotoEnd();
                else
                    game_.redo();
                break;

            case Key::Home:
                game_.gotoStart();
                break;

            case Key::End:
                game_.gotoEnd();
                break;
        }

        // Any seek invalidates an in-flight drag or promotion: the drag
        // may be anchored to a piece that no longer exists in the new
        // position, and the pending promotion's from/to squares may not
        // be legal anymore.
        drag_ = {};
        promo_ = {};
        cancelSelection();
    }

    void GuiGameController::playMoveOrStartPromotion(Square from, Square to, bool fromDrag)
    {
        if (isFrozen(game_))
            return;

        if (isPromotionMove(from, to))
        {
            lastMoveWasDrag_ = fromDrag;

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

            result_ = {ActionResult::Kind::PromotionPending, from, to, game_.pieceAt(from)};
            cancelSelection();
            return;
        }

        if (game_.play(from, to))
        {
            lastMoveWasDrag_ = fromDrag;
            result_ = {ActionResult::Kind::MovePlayed, from, to};
        }
        else
        {
            result_ = {ActionResult::Kind::IllegalMove, from, to};
        }

        cancelSelection();
    }

    void GuiGameController::completePromotion(PieceType type)
    {
        if (!promo_.active)
            return;

        if (isFrozen(game_))
        {
            promo_ = {};
            return;
        }

        const Square from = promo_.from;
        const Square to = promo_.to;

        promo_ = {};

        if (game_.play(from, to, type))
            result_ = {ActionResult::Kind::MovePlayed, from, to};
        else
            result_ = {ActionResult::Kind::IllegalMove, from, to};

        cancelSelection();
    }

    void GuiGameController::cancelPromotion()
    {
        if (!promo_.active)
            return;

        const Square from = promo_.from;
        const Square to = promo_.to;

        promo_ = {};
        result_ = {ActionResult::Kind::MoveCancelled, from, to};

        cancelSelection();
    }

    void GuiGameController::applyQueuedActions()
    {
        const bool hadQueued = queuedGotoPly_.has_value() || queuedUndo_ || queuedRedo_;

        if (!hadQueued)
            return;

        if (isFrozen(game_))
        {
            queuedGotoPly_.reset();
            queuedUndo_ = false;
            queuedRedo_ = false;
            return;
        }

        lastMoveWasDrag_ = false;

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
        cancelSelection();
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

    void GuiGameController::requestNewGame()
    {
        newGameRequested_ = true;
    }

    bool GuiGameController::consumeNewGameRequest()
    {
        const bool requested = newGameRequested_;
        newGameRequested_ = false;
        return requested;
    }

    void GuiGameController::requestResign(Color side)
    {
        queuedResign_ = side;
    }

    std::optional<Color> GuiGameController::consumeResignRequest()
    {
        const auto result = queuedResign_;
        queuedResign_.reset();
        return result;
    }

    void GuiGameController::requestQuit()
    {
        quitRequested_ = true;
    }

    bool GuiGameController::consumeQuitRequest()
    {
        const bool requested = quitRequested_;
        quitRequested_ = false;
        return requested;
    }

    void GuiGameController::addOrReplaceArrow(Square from, Square to)
    {
        // Toggle: drawing the same arrow twice removes it.
        for (auto it = arrows_.begin(); it != arrows_.end(); ++it)
        {
            if (it->from == from && it->to == to)
            {
                arrows_.erase(it);
                return;
            }
        }

        // One arrow per tail: a new arrow from the same source replaces
        // the existing one rather than stacking. Keeps the board readable.
        for (auto it = arrows_.begin(); it != arrows_.end(); ++it)
        {
            if (it->from == from)
            {
                it->to = to;
                return;
            }
        }

        // Soft cap. Oldest arrow drops off to make room, matching the
        // way a single-finger drawing app behaves.
        constexpr std::size_t kMaxArrows = 8;
        if (arrows_.size() >= kMaxArrows)
            arrows_.erase(arrows_.begin());

        arrows_.push_back({from, to});
    }

    bool GuiGameController::eraseArrowFrom(Square from)
    {
        for (auto it = arrows_.begin(); it != arrows_.end(); ++it)
        {
            if (it->from == from)
            {
                arrows_.erase(it);
                return true;
            }
        }

        return false;
    }

} // namespace Gui