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

    Square Board::uciToSquare(std::string uci)
    {
        if (uci.length() < 2)
            return 0;
        int file = uci[0] - 'a';
        int rank = uci[1] - '1';
        return static_cast<Square>(rank * 8 + file);
    }

    const UCI Board::SquareToUci(Square square)
    {
        int file = square % 8;
        int rank = square / 8;
        std::string uci = "";
        uci += static_cast<char>('a' + file);
        uci += static_cast<char>('1' + rank);
        return uci;
    }

    void Board::setupInitialBoard()
    {
        for (int i = 0; i < 64; ++i)
        {
            squares[i] = std::nullopt;
        }

        for (int file = 0; file < 8; ++file)
        {
            squares[8 + file] = Piece{Color::White, PieceType::Pawn};
            squares[48 + file] = Piece{Color::Black, PieceType::Pawn};
        }

        PieceType backline[8] = {PieceType::Rook, PieceType::Knight, PieceType::Bishop, PieceType::Queen,
                                 PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook};

        for (int file = 0; file < 8; ++file)
        {
            squares[file] = Piece{Color::White, backline[file]};
            squares[56 + file] = Piece{Color::Black, backline[file]};
        }
    }

} // namespace Chess