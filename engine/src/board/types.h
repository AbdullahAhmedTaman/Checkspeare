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

struct Score
{
    int mg[2] = {0, 0};
    int eg[2] = {0, 0};

    Score() = default;

    Score& operator+=(const Score& rhs)
    {
        mg[WHITE] += rhs.mg[WHITE];
        mg[BLACK] += rhs.mg[BLACK];
        eg[WHITE] += rhs.eg[WHITE];
        eg[BLACK] += rhs.eg[BLACK];
        return *this;
    }

    Score& operator-=(const Score& rhs)
    {
        mg[WHITE] -= rhs.mg[WHITE];
        mg[BLACK] -= rhs.mg[BLACK];
        eg[WHITE] -= rhs.eg[WHITE];
        eg[BLACK] -= rhs.eg[BLACK];
        return *this;
    }
};

/**
 * @brief High-performance 32-bit packed Move structure.
 *
 * Memory Layout:
 *   Bits  0 -  5: Source square (Square: 0..63)
 *   Bits  6 - 11: Destination square (Square: 0..63)
 *   Bits 12 - 15: Move flag / metadata (MoveFlag: 0..15)
 *   Bits 16 - 31: Move ordering heuristic score (int16_t: -32768..32767)
 */
struct alignas(4) Move
{
    uint16_t data = 0; ///< Bits 0-5: from, 6-11: to, 12-15: flag
    int16_t score = 0; ///< Bits 16-31: transient search ordering score

    Move() = default;

    /**
     * @brief Constructs a Move with source, target, flag, and optional ordering score.
     * @param from Source square on the 8x8 grid.
     * @param to Destination square on the 8x8 grid.
     * @param moveFlag Encoded movement attribute (captures, promotions, castles).
     * @param moveScore Initial ordering priority score (defaults to 0).
     */
    constexpr Move(Square from, Square to, MoveFlag moveFlag, int16_t moveScore = 0)
        : data(static_cast<uint16_t>(static_cast<uint16_t>(from) | (static_cast<uint16_t>(to) << 6) |
                                     (static_cast<uint16_t>(moveFlag) << 12))),
          score(moveScore)
    {
        assert(is_valid(from) && is_valid(to));
    }

    /**
     * @brief Explicitly reconstructs a Move from a raw 16-bit bitmask.
     *        Used during TT unpack and bitstream operations.
     */
    explicit constexpr Move(uint16_t raw_data, int16_t moveScore = 0)
        : data(raw_data),
          score(moveScore)
    {
    }

    [[nodiscard]] constexpr uint16_t raw() const noexcept
    {
        return data;
    }

    [[nodiscard]] constexpr int16_t ordering_score() const noexcept
    {
        return score;
    }

    constexpr void set_score(int16_t s) noexcept
    {
        score = s;
    }

    static constexpr Move none()
    {
        return Move();
    }

    static constexpr Move null()
    {
        return Move(A1, A1, NULL_MOVE, 0);
    }

    [[nodiscard]] constexpr Square from() const
    {
        return static_cast<Square>(data & 0x3F);
    }

    [[nodiscard]] constexpr Square to() const
    {
        return static_cast<Square>((data >> 6) & 0x3F);
    }

    [[nodiscard]] constexpr int flag() const
    {
        return (data >> 12) & 0xF;
    }

    [[nodiscard]] constexpr bool is_ok() const
    {
        return data != 0;
    }

    [[nodiscard]] constexpr bool is_null() const
    {
        return flag() == NULL_MOVE;
    }

    [[nodiscard]] constexpr bool is_capture() const
    {
        return flag() != NULL_MOVE && (flag() & 4) != 0;
    }

    [[nodiscard]] constexpr bool is_promotion() const
    {
        return flag() >= KNIGHT_PROMO;
    }

    [[nodiscard]] constexpr bool is_castle() const
    {
        return flag() == KING_CASTLE || flag() == QUEEN_CASTLE;
    }

    [[nodiscard]] constexpr bool is_kingside_castle() const
    {
        return flag() == KING_CASTLE;
    }

    [[nodiscard]] constexpr bool is_queenside_castle() const
    {
        return flag() == QUEEN_CASTLE;
    }

    [[nodiscard]] constexpr bool is_ep() const
    {
        return flag() == EP_CAPTURE;
    }

    [[nodiscard]] constexpr bool is_double_push() const
    {
        return flag() == DOUBLE_PAWN_PUSH;
    }

    /**
     * @brief Strict move identity comparison.
     * Compares ONLY the 16-bit spatial transition to ensure TT hits and killers
     * are evaluated independently of transient score mutations.
     */
    inline bool operator==(const Move& m) const noexcept
    {
        return data == m.data;
    }

    inline bool operator!=(const Move& m) const noexcept
    {
        return data != m.data;
    }
};

static_assert(sizeof(Move) == 4, "Move struct must remain exactly 32 bits (4 bytes).");
static_assert(alignof(Move) == 2 || alignof(Move) == 4, "Move alignment must fit register bounds.");

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
    uint64_t max_nodes = UINT64_MAX;
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