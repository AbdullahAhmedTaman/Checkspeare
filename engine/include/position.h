#pragma once
#include "types.h"

struct Position
{
    Bitboard pieces[2][PieceType::NONE]; //[color][pieceType]
    Bitboard occupancy[3];

    Piece board[Square::SQ_NONE];

    Color sideToMove;

    uint8_t castlingRights;
    Square enPassantSquare;

    uint16_t halfmoveClock;

    // Zobrist hash
    Key key;
};