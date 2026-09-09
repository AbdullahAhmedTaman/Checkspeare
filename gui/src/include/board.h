#pragma once
#include <array>
#include <cstdint>
#include <optional>

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

    class Board
    {
        const std ::optional<Piece>& getPiece(Square square) const;
        void setPiece(Square square, std::optional<Piece> piece);

    private:
        std::array<std::optional<Piece>, 64> squares;
    };
} // namespace Chess