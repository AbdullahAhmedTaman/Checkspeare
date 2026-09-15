#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace Chess
{
    enum class Color
    {
        Black,
        White
    };
    enum class PieceType
    {
        Pawn,
        Knight,
        Bishop,
        Rook,
        Queen,
        King
    };

    struct Piece
    {
        Color color;
        PieceType type;
    };

    using Square = std::uint8_t;
    using UCI = std::string;

    class Board
    {
    public:
        Board() = default;
        void setupInitialBoard();
        const std ::optional<Piece>& getPiece(Square square) const;
        void setPiece(Square square, std::optional<Piece> piece);
        Square uciToSquare(std::string);
        static UCI SquareToUci(Square square);
        static std::string getPieceName(const Piece& piece);
        static std::string getSquareColor(Square square);

    private:
        std::array<std::optional<Piece>, 64> squares;
    };
} // namespace Chess