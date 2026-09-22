#include "BoardAnnotations.hpp"
#include <cstddef>

namespace Gui
{

    void BoardAnnotations::clearSelection()
    {
        selected.reset();
        legalTargets.clear();
    }

    void BoardAnnotations::refreshFrom(const ChessGame& game)
    {
        const std::size_t cursor = game.cursor();
        if (cursor == 0)
        {
            lastMoveFrom.reset();
            lastMoveTo.reset();
        }
        else
        {
            const Move& m = game.history()[cursor - 1].move;
            lastMoveFrom = m.from();
            lastMoveTo = m.to();
        }

        checkedKing.reset();
        const Board& board = game.board();
        if (board.inCheck())
        {
            const Color stm = board.sideToMove();
            for (int i = 0; i < 64; ++i)
            {
                const Square sq = static_cast<Square>(i);
                const Piece pc = board.pieceAt(sq);
                if (pc != PIECE_NONE && type_of(pc) == KING && color_of(pc) == stm)
                {
                    checkedKing = sq;
                    break;
                }
            }
        }
    }

} // namespace Gui