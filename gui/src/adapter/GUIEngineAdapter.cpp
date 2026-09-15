#pragma once

#include "movegen.h"
#include "position.h"
#include "types.h"
#include <vector>

class GUIEngineAdapter
{
private:
    Position pos;
    MoveList currentLegalMoves;

    void refreshLegalMoves()
    {
        currentLegalMoves.count = 0;
        MoveGen::generate_legal_moves(pos, currentLegalMoves);
    }

public:
    GUIEngineAdapter()
    {
        pos.clear();
        refreshLegalMoves();
    }

    void loadFEN(const std::string& fen)
    {
        pos = Position::from_fen(fen);
        refreshLegalMoves();
    }

    Piece getPieceAt(Square sq) const
    {
        if (!is_valid(sq))
            return PIECE_NONE;
        return pos.board[sq];
    }

    Color getSideToMove() const
    {
        return pos.sideToMove;
    }

    bool isCheckmate() const
    {
        Color opponent = (pos.sideToMove == WHITE) ? BLACK : WHITE;
        return currentLegalMoves.size() == 0 && pos.is_square_attacked(pos.king_square(pos.sideToMove), opponent);
    }

    bool isStalemate() const
    {
        Color opponent = (pos.sideToMove == WHITE) ? BLACK : WHITE;
        return currentLegalMoves.size() == 0 && !pos.is_square_attacked(pos.king_square(pos.sideToMove), opponent);
    }

    std::vector<Square> getLegalDestinations(Square fromSq) const
    {
        std::vector<Square> dests;
        for (const Move* m = currentLegalMoves.begin(); m != currentLegalMoves.end(); ++m)
        {
            if (m->from() == fromSq)
            {
                dests.push_back(m->to());
            }
        }
        return dests;
    }

    std::vector<Move> getLegalMovesFrom(Square fromSq) const
    {
        std::vector<Move> moves;
        for (const Move* m = currentLegalMoves.begin(); m != currentLegalMoves.end(); ++m)
        {
            if (m->from() == fromSq)
            {
                moves.push_back(*m);
            }
        }
        return moves;
    }

    bool attemptMove(Square from, Square to, PieceType promoType = QUEEN)
    {
        if (!is_valid(from) || !is_valid(to))
            return false;

        for (const Move* m = currentLegalMoves.begin(); m != currentLegalMoves.end(); ++m)
        {
            if (m->from() == from && m->to() == to)
            {
                if (m->is_promotion())
                {
                    if (promotion_type(*m) != promoType)
                    {
                        continue;
                    }
                }

                pos.make_move(*m);
                refreshLegalMoves();
                return true;
            }
        }

        return false;
    }

    static Square coordsToSquare(int gridX, int gridY, bool isBoardFlipped = false)
    {
        if (gridX < 0 || gridX > 7 || gridY < 0 || gridY > 7)
            return SQ_NONE;

        if (isBoardFlipped)
        {
            gridX = 7 - gridX;
            gridY = 7 - gridY;
        }

        int rank = 7 - gridY;
        int file = gridX;

        return static_cast<Square>(rank * 8 + file);
    }
};