#pragma once

#include "board/types.h"
#include "game/BoardGeometry.hpp"
#include "game/ChessGame.hpp"
#include "input/InputEvent.hpp"
#include "ui/board/BoardAnnotations.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

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

        /// Installs the predicate the controller consults before allowing
        /// a human-originated mutation (move, promotion commit). Returns
        /// true when `side` is allowed to be moved by a human in the
        /// current setup. Defaults to "always allowed" so the controller
        /// works standalone in tests or other front-ends.
        void setHumanMayMove(std::function<bool(Color)> pred)
        {
            humanMayMove_ = std::move(pred);
        }

        /// Installs the predicate the controller consults to decide whether
        /// an engine is currently playing `side`. Used by sideIsHuman() and
        /// canResign(). Defaults to "no engine controls any side", which is
        /// correct for tests and headless use.
        void setSideIsEngineControlled(std::function<bool(Color)> pred)
        {
            sideIsEngineControlled_ = std::move(pred);
        }

        void handleEvent(const InputEvent& event, const BoardGeometry& geo);

        /// True when the controller wants this event delivered, given
        /// whether the pointer is currently over the board panel.
        /// Encapsulates the arrow-gesture routing rules so the caller does
        /// not need to inspect arrows()/drag() to decide what to forward.
        bool wantsEvent(const InputEvent& ev, bool insideBoard) const;

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

        /// Queues a FEN load. Application drains it in stepSimulation,
        /// aborts both engines (they may be mid-search on the old position),
        /// resets the board, and lets each EnginePlayer re-evaluate whether
        /// to start a fresh search on the new position.
        void requestLoadFen(std::string fen);
        std::optional<std::string> consumeLoadFenRequest();

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

        /// True when the human side to move may resign. Only consults state
        /// the controller can see: the game result and the engine-control
        /// predicate. Callers that also gate on a clock flag should check
        /// that separately before delegating here.
        bool canResign() const;

        /// True if no engine is configured to play `side`.
        bool sideIsHuman(Color side) const;

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
        std::optional<std::string> queuedLoadFen_;
        bool newGameRequested_ = false;
        bool gameOverModalDismissed_ = false;
        bool quitRequested_ = false;

        /// Authority gate for human-originated mutations. Consulted at the
        /// point a move is about to be committed, not in input routing.
        std::function<bool(Color)> humanMayMove_ = [](Color)
        {
            return true;
        };

        /// Engine-ownership query. Consulted by sideIsHuman() and
        /// canResign(). Defaults to "no engines", which makes every side
        /// human — correct for tests and standalone use.
        std::function<bool(Color)> sideIsEngineControlled_ = [](Color)
        {
            return false;
        };
    };

} // namespace Gui