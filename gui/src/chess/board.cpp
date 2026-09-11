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

    std::string Board::SquareToUci(Square square)
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

    std::string Board::getPieceName(const Piece& piece)
    {
        std::string colorStr = (piece.color == Color::White) ? "White" : "Black";
        std::string typeStr;

        switch (piece.type)
        {
            case PieceType::Pawn:
                typeStr = "Pawn";
                break;
            case PieceType::Knight:
                typeStr = "Knight";
                break;
            case PieceType::Bishop:
                typeStr = "Bishop";
                break;
            case PieceType::Rook:
                typeStr = "Rook";
                break;
            case PieceType::Queen:
                typeStr = "Queen";
                break;
            case PieceType::King:
                typeStr = "King";
                break;
        }

        return colorStr + " " + typeStr;
    }

    std::string Board::getSquareColor(Square square)
    {
        int file = square % 8;
        int rank = square / 8;
        return ((rank + file) % 2 == 0) ? "Dark" : "Light";
    }

} // namespace Chess