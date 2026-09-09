#include "../include/board.h"
#include <optional>

namespace Chess
{
    const std ::optional<Piece>& Board::getPiece(Square square) const
    {
        return squares[square];
    }

    void Board::setPiece(Square square, std::optional<Piece> piece)
    {
        squares[square] = piece;
    }
} // namespace Chess