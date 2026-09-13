#include "types.h"
#include <ostream>

std::ostream& operator<<(std::ostream& os, Color c)
{
    return os << (c == WHITE ? "White" : c == BLACK ? "Black" : "Both");
}

std::ostream& operator<<(std::ostream& os, Piece p)
{
    // FEN piece letters. PIECE_NONE is '.'.
    static constexpr char letters[] = "PNBRQKpnbrqk.";
    return os << letters[static_cast<int>(p)];
}

std::ostream& operator<<(std::ostream& os, Square s)
{
    if (!is_valid(s))
        return os << "none";
    return os << static_cast<char>('a' + (s & 7)) << static_cast<char>('1' + (s >> 3));
}

std::ostream& operator<<(std::ostream& os, MoveFlag f)
{
    switch (f)
    {
        case QUIET_MOVE:
            return os << "quiet";
        case DOUBLE_PAWN_PUSH:
            return os << "double-push";
        case KING_CASTLE:
            return os << "O-O";
        case QUEEN_CASTLE:
            return os << "O-O-O";
        case CAPTURE:
            return os << "capture";
        case EP_CAPTURE:
            return os << "ep-capture";
        case NULL_MOVE:
            return os << "null";
        case KNIGHT_PROMO:
            return os << "=N";
        case BISHOP_PROMO:
            return os << "=B";
        case ROOK_PROMO:
            return os << "=R";
        case QUEEN_PROMO:
            return os << "=Q";
        case KNIGHT_PROMO_CAP:
            return os << "=N#";
        case BISHOP_PROMO_CAP:
            return os << "=B#";
        case ROOK_PROMO_CAP:
            return os << "=R#";
        case QUEEN_PROMO_CAP:
            return os << "=Q#";
    }
    return os << "?";
}

std::ostream& operator<<(std::ostream& os, const Move& m)
{
    if (m.is_null())
        return os << "0000";
    if (!m.is_ok())
        return os << "<none>";

    os << m.from() << m.to();
    if (m.is_promotion())
    {
        // UCI-style promotion letters. Indices 0-7 are unreachable; is_promotion() gates the call.
        static constexpr char promoChar[] = "........nbrqnbrq";
        os << promoChar[m.flag()];
    }
    return os;
}