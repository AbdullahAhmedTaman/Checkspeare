#pragma once
#include <cstdint>

enum Color { WHITE, BLACK, BOTH };

enum Piece { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE };

enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE
};


// 16-bit encoding:
// 0000 0000 0011 1111 -> bits 0-5: source square (0-63)
// 0000 1111 1100 0000 -> bits 6-11: target square (0-63)
// 0011 0000 0000 0000 -> bits 12-13: promotion piece type
// 1100 0000 0000 0000 -> bits 14-15: special move flags (castling, en passant)
struct Move {
    uint16_t data;

    inline Square from() const { return static_cast<Square>(data & 0x3F); }
    inline Square to()   const { return static_cast<Square>((data >> 6) & 0x3F); }
    inline int flag()    const { return (data >> 12) & 0xF; }
    
    inline bool operator==(const Move& m) const { return data == m.data; }
    inline bool operator!=(const Move& m) const { return data != m.data; }
};


struct SearchLimits {
    int depth;
    int wtime, btime;
    int winc, binc;
    int movetime;
    bool infinite;
};