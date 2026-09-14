#pragma once
#include <cassert>
#include <cstdint>
#include <iosfwd>

using Bitboard = uint64_t;
using Key = uint64_t;
using Value = int;

constexpr int MAX_PLY = 128;
constexpr Value VALUE_ZERO = 0;
constexpr Value VALUE_MATE = 32000;
constexpr Value VALUE_INFINITE = 32001;

enum Color : uint8_t
{
    WHITE,
    BLACK,
    BOTH
};

enum PieceType : uint8_t
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE
};

enum Piece : uint8_t
{
    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,

    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING,

    PIECE_NONE
};

constexpr PieceType PIECE_TO_TYPE[13] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE};

inline constexpr PieceType type_of(Piece pc)
{
    return PIECE_TO_TYPE[pc];
}

constexpr Color color_of(Piece pc)
{
    return pc < BLACK_PAWN ? WHITE : BLACK;
}

// Color and piece type packed into a Piece. Assumes 6 piece types per color.
constexpr Piece make_piece(Color c, PieceType pt)
{
    return static_cast<Piece>(static_cast<int>(c) * 6 + static_cast<int>(pt));
}

// clang-format off
enum Square : uint8_t
{
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

// clang-format on

constexpr bool is_valid(Square s)
{
    return s < SQ_NONE;
}

enum MoveFlag
{
    QUIET_MOVE = 0,
    DOUBLE_PAWN_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    // 6 unused
    NULL_MOVE = 7,
    KNIGHT_PROMO = 8,
    BISHOP_PROMO = 9,
    ROOK_PROMO = 10,
    QUEEN_PROMO = 11,
    KNIGHT_PROMO_CAP = 12,
    BISHOP_PROMO_CAP = 13,
    ROOK_PROMO_CAP = 14,
    QUEEN_PROMO_CAP = 15
};

// 16-bit encoding:
// bits 0-5: from square, bits 6-11: to square, bits 12-15: flag.
struct Move
{
    uint16_t data;

    Move() = default;

    constexpr Move(Square from, Square to, MoveFlag moveFlag)
        : data(from | (to << 6) | (moveFlag << 12))
    {
        assert(is_valid(from) && is_valid(to));
    }

    // data == 0, never a legal move.
    static constexpr Move none()
    {
        return Move();
    }

    static constexpr Move null()
    {
        return Move(A1, A1, NULL_MOVE);
    }

    constexpr Square from() const
    {
        return static_cast<Square>(data & 0x3F);
    }

    constexpr Square to() const
    {
        return static_cast<Square>((data >> 6) & 0x3F);
    }

    constexpr int flag() const
    {
        return (data >> 12) & 0xF;
    }

    constexpr bool is_ok() const
    {
        return data != 0;
    }

    constexpr bool is_null() const
    {
        return flag() == NULL_MOVE;
    }

    // Includes EP captures, whose victim is not on the destination square.
    constexpr bool is_capture() const
    {
        return flag() != NULL_MOVE && (flag() & 4) != 0;
    }

    constexpr bool is_promotion() const
    {
        return flag() >= KNIGHT_PROMO;
    }

    constexpr bool is_castle() const
    {
        return flag() == KING_CASTLE || flag() == QUEEN_CASTLE;
    }

    constexpr bool is_kingside_castle() const
    {
        return flag() == KING_CASTLE;
    }

    constexpr bool is_queenside_castle() const
    {
        return flag() == QUEEN_CASTLE;
    }

    constexpr bool is_ep() const
    {
        return flag() == EP_CAPTURE;
    }

    constexpr bool is_double_push() const
    {
        return flag() == DOUBLE_PAWN_PUSH;
    }

    inline bool operator==(const Move& m) const
    {
        return data == m.data;
    }

    inline bool operator!=(const Move& m) const
    {
        return data != m.data;
    }
};

// Precondition: m.is_promotion().
constexpr PieceType promotion_type(const Move& m)
{
    switch (m.flag())
    {
        case KNIGHT_PROMO:
        case KNIGHT_PROMO_CAP:
            return KNIGHT;
        case BISHOP_PROMO:
        case BISHOP_PROMO_CAP:
            return BISHOP;
        case ROOK_PROMO:
        case ROOK_PROMO_CAP:
            return ROOK;
        default:
            return QUEEN;
    }
}

std::ostream& operator<<(std::ostream& os, Color c);
std::ostream& operator<<(std::ostream& os, Piece p);
std::ostream& operator<<(std::ostream& os, Square s);
std::ostream& operator<<(std::ostream& os, MoveFlag f);
std::ostream& operator<<(std::ostream& os, const Move& m);

struct SearchLimits
{
    int depth = MAX_PLY;
    int wtime = 0, btime = 0;
    int winc = 0, binc = 0;
    int movetime = 0;
    uint64_t max_nodes = UINT64_MAX; // Needed for UCI 'go nodes X'
    bool infinite = false;
};

template <typename T>
struct EnumRange
{
    T first;
    T last;

    struct Iterator
    {
        T value;

        T operator*() const
        {
            return value;
        }

        Iterator& operator++()
        {
            value = static_cast<T>(static_cast<uint8_t>(value) + 1);
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return value != other.value;
        }
    };

    Iterator begin() const
    {
        return {first};
    }

    Iterator end() const
    {
        return {last};
    }
};

template <typename T>
constexpr EnumRange<T> all_values(T start, T end)
{
    return {start, end};
}

constexpr auto all_squares()
{
    return all_values(A1, SQ_NONE);
}

constexpr auto all_piece_types()
{
    return all_values(PAWN, NONE);
}