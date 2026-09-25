#include "San.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    char pieceLetter(PieceType t)
    {
        switch (t)
        {
            case KNIGHT:
                return 'N';

            case BISHOP:
                return 'B';

            case ROOK:
                return 'R';

            case QUEEN:
                return 'Q';

            case KING:
                return 'K';

            case PAWN:
                return '\0';

            default:
                return '\0';
        }
    }

    char fileChar(Square sq)
    {
        return static_cast<char>('a' + (static_cast<int>(sq) % 8));
    }

    char rankChar(Square sq)
    {
        return static_cast<char>('1' + (static_cast<int>(sq) / 8));
    }

    struct Disambiguation
    {
        bool needed = false;
        bool useFile = false;
        bool useRank = false;
        bool useFull = false;
    };

    Disambiguation computeDisambiguation(const Board& board, Move m, PieceType type, Color side)
    {
        Disambiguation d;

        const Square from = m.from();
        const Square to = m.to();

        std::vector<Square> others;

        for (const Move& other : board.allLegalMoves())
        {
            if (other.to() != to)
                continue;

            if (other.from() == from)
                continue;

            const Piece pc = board.pieceAt(other.from());

            if (type_of(pc) != type)
                continue;

            if (color_of(pc) != side)
                continue;

            others.push_back(other.from());
        }

        if (others.empty())
            return d;

        d.needed = true;

        const bool fileCollides = std::any_of(others.begin(), others.end(),
                                              [from](Square s)
                                              {
                                                  return fileChar(s) == fileChar(from);
                                              });

        const bool rankCollides = std::any_of(others.begin(), others.end(),
                                              [from](Square s)
                                              {
                                                  return rankChar(s) == rankChar(from);
                                              });

        if (!fileCollides)
            d.useFile = true;
        else if (!rankCollides)
            d.useRank = true;
        else
            d.useFull = true;

        return d;
    }
} // namespace

std::string toSan(Board board, Move m)
{
    if (m.is_castle())
    {
        std::string san = (fileChar(m.to()) == 'g') ? "O-O" : "O-O-O";

        board.applyMove(m);

        if (board.isCheckmate())
            san += '#';
        else if (board.inCheck())
            san += '+';

        return san;
    }

    const Piece movingPiece = board.pieceAt(m.from());
    const PieceType type = type_of(movingPiece);
    const Color side = color_of(movingPiece);

    std::string san;

    if (type == PAWN)
    {
        // En passant has an empty destination square, so check is_ep()
        // explicitly instead of relying only on destination occupancy.
        if (m.is_capture() || m.is_ep())
        {
            san += fileChar(m.from());
            san += 'x';
        }

        san += fileChar(m.to());
        san += rankChar(m.to());

        if (m.is_promotion())
        {
            san += '=';
            san += pieceLetter(promotion_type(m));
        }
    }
    else
    {
        san += pieceLetter(type);

        if (type != KING)
        {
            const Disambiguation d = computeDisambiguation(board, m, type, side);

            if (d.useFile)
                san += fileChar(m.from());

            if (d.useRank)
                san += rankChar(m.from());

            if (d.useFull)
            {
                san += fileChar(m.from());
                san += rankChar(m.from());
            }
        }

        if (m.is_capture())
            san += 'x';

        san += fileChar(m.to());
        san += rankChar(m.to());
    }

    board.applyMove(m);

    if (board.isCheckmate())
        san += '#';
    else if (board.inCheck())
        san += '+';

    return san;
}