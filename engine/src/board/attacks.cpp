#include "attacks.h"
#include "bitboard.h"
#include "types.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

// Definitions of the tables declared extern in attacks.h
namespace attacks_detail
{
    Bitboard arrKnight[Square::SQ_NONE];
    Bitboard arrKing[Square::SQ_NONE];
    Bitboard arrPawn[2][Square::SQ_NONE];

#if defined(USE_PEXT)
    PextEntry bishopEntries[64];
#if defined(ATTACKS_ROOK_COMPRESSED)
    RookEntry rookEntries[64];
#else
    PextEntry rookEntries[64];
#endif
#endif
} // namespace attacks_detail

namespace
{
    Bitboard arrReverseSquareBit[Square::SQ_NONE]; // used by the HQ code

    // Non-sliding tables (init only)
    Bitboard calculate_knight_attacks(Square sq)
    {
        Bitboard bb = square_bb(sq);
        Bitboard attacks = 0;

        attacks |= shift_north(shift_north(shift_east(bb)));
        attacks |= shift_north(shift_north(shift_west(bb)));

        attacks |= shift_south(shift_south(shift_east(bb)));
        attacks |= shift_south(shift_south(shift_west(bb)));

        attacks |= shift_east(shift_east(shift_north(bb)));
        attacks |= shift_east(shift_east(shift_south(bb)));

        attacks |= shift_west(shift_west(shift_north(bb)));
        attacks |= shift_west(shift_west(shift_south(bb)));

        return attacks;
    }

    Bitboard calculate_king_attacks(Square sq)
    {
        Bitboard bb = square_bb(sq);

        return shift_north(bb) | shift_south(bb) | shift_east(bb) | shift_west(bb) | shift_north_east(bb) | shift_south_east(bb) |
               shift_north_west(bb) | shift_south_west(bb);
    }

    Bitboard calculate_pawn_attacks(Color color, Square sq)
    {
        Bitboard bb = square_bb(sq);

        if (color == Color::WHITE)
            return shift_north_east(bb) | shift_north_west(bb);

        return shift_south_east(bb) | shift_south_west(bb);
    }

    // HQ (Hyperbola Quintessence): the fallback backend
    inline Bitboard ray_attacks(Bitboard occ, Bitboard mask, Square sq)
    {
        Bitboard s = square_bb(sq);
        Bitboard o = occ & mask;
        Bitboard forward = o - (s << 1);
        Bitboard rev = reverse_bb(reverse_bb(o) - (arrReverseSquareBit[sq] << 1));
        return (forward ^ rev) & mask;
    }

    inline Bitboard hq_bishop_attacks(Bitboard occ, Square sq)
    {
        return ray_attacks(occ, diagonal_bb(sq), sq) | ray_attacks(occ, anti_diagonal_bb(sq), sq);
    }

    inline Bitboard hq_rook_attacks(Bitboard occ, Square sq)
    {
        return ray_attacks(occ, file_bb(sq), sq) | ray_attacks(occ, rank_bb(sq), sq);
    }

#if defined(USE_PEXT)

    // Helpers for building tables (init-only)
    constexpr int kRookDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // {dFile, dRank}
    constexpr int kBishopDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    inline bool on_board(int f, int r)
    {
        return f >= 0 && f < 8 && r >= 0 && r < 8;
    }

    // Naive reference: walk each ray, stop at (and include) the first blocker.
    inline Bitboard slow_attacks(Square sq, Bitboard occ, const int (&dirs)[4][2])
    {
        Bitboard attacks = 0;
        for (const auto& d : dirs)
        {
            int f = static_cast<int>(file_of(sq)) + d[0];
            int r = static_cast<int>(rank_of(sq)) + d[1];
            while (on_board(f, r))
            {
                const Bitboard b = Bitboard(1) << (r * 8 + f);
                attacks |= b;
                if (occ & b)
                    break;
                f += d[0];
                r += d[1];
            }
        }
        return attacks;
    }

    // Squares whose occupancy can change the attack set: every ray square except
    // the last one on each ray.
    inline Bitboard relevant_mask(Square sq, const int (&dirs)[4][2])
    {
        Bitboard mask = 0;
        for (const auto& d : dirs)
        {
            int f = static_cast<int>(file_of(sq)) + d[0];
            int r = static_cast<int>(rank_of(sq)) + d[1];
            while (on_board(f + d[0], r + d[1]))
            {
                mask |= Bitboard(1) << (r * 8 + f);
                f += d[0];
                r += d[1];
            }
        }
        return mask;
    }

    // Software PDEP: scatter the low bits of src into the set-bit positions of mask.
    inline Bitboard pdep_soft(Bitboard src, Bitboard mask)
    {
        Bitboard result = 0;
        for (Bitboard bit = 1; mask != 0; bit <<= 1)
        {
            const Bitboard lowest = mask & (~mask + 1); // isolate lowest set bit of mask
            if (src & bit)
                result |= lowest; // src bit k -> k-th set bit of mask
            mask &= mask - 1;     // move on to the next mask bit
        }
        return result;
    }

#endif // USE_PEXT

#if defined(USE_PEXT)

    using attacks_detail::PextEntry;

    alignas(64) Bitboard bishopStorage[5248];

#if defined(ATTACKS_ROOK_COMPRESSED)
    alignas(64) std::uint16_t rookStorage16[102400];
#else
    alignas(64) Bitboard rookStorage[102400];
#endif

    // Fills the 64 tables for one piece type (full 64-bit entries).
    void init_pext_table(PextEntry* entries, Bitboard* storage, std::size_t capacity, const int (&dirs)[4][2])
    {
        Bitboard* next = storage;
        for (Square sq : all_squares())
        {
            PextEntry& e = entries[sq];
            e.mask = relevant_mask(sq, dirs);
            e.table = next;

            const std::size_t size = std::size_t(1) << popcount(e.mask);
            assert(next + size <= storage + capacity);

            for (std::size_t i = 0; i < size; ++i)
            {
                const Bitboard occ = pdep_soft(i, e.mask); // the i-th blocker pattern
                e.table[i] = slow_attacks(sq, occ, dirs);  // its true attack set
            }
            next += size;
        }
        assert(next == storage + capacity);
        (void) capacity;
    }

#if defined(ATTACKS_ROOK_COMPRESSED)
    // Rook table with 16-bit entries: the attack set is always a subset of the
    // 14 empty-board rook squares, so it can be squeezed into 14 bits with PEXT
    // and expanded at lookup time with PDEP.
    void init_rook_compressed()
    {
        constexpr std::size_t capacity = sizeof(rookStorage16) / sizeof(std::uint16_t);
        std::uint16_t* next = rookStorage16;

        for (Square sq : all_squares())
        {
            attacks_detail::RookEntry& e = attacks_detail::rookEntries[sq];
            e.mask = relevant_mask(sq, kRookDirs);
            e.rays = slow_attacks(sq, 0, kRookDirs); // empty board: all 14 squares
            e.table = next;

            const std::size_t size = std::size_t(1) << popcount(e.mask);
            assert(next + size <= rookStorage16 + capacity);

            for (std::size_t i = 0; i < size; ++i)
            {
                const Bitboard occ = pdep_soft(i, e.mask);
                const Bitboard att = slow_attacks(sq, occ, kRookDirs); // subset of e.rays
                e.table[i] = static_cast<std::uint16_t>(_pext_u64(att, e.rays));
            }
            next += size;
        }
        assert(next == rookStorage16 + capacity);
    }
#endif

    void init_pext_attacks()
    {
        init_pext_table(attacks_detail::bishopEntries, bishopStorage, sizeof(bishopStorage) / sizeof(Bitboard), kBishopDirs);
#if defined(ATTACKS_ROOK_COMPRESSED)
        init_rook_compressed();
#else
        init_pext_table(attacks_detail::rookEntries, rookStorage, sizeof(rookStorage) / sizeof(Bitboard), kRookDirs);
#endif
    }

#endif // USE_PEXT

} // namespace

/**
 * @brief Have to be called once at the start of the engine to init attacks.
 *
 */
void init_attacks()
{
    for (Square sq : all_squares())
    {
        attacks_detail::arrKnight[sq] = calculate_knight_attacks(sq);
        attacks_detail::arrKing[sq] = calculate_king_attacks(sq);
        attacks_detail::arrPawn[static_cast<int>(Color::WHITE)][sq] = calculate_pawn_attacks(Color::WHITE, sq);
        attacks_detail::arrPawn[static_cast<int>(Color::BLACK)][sq] = calculate_pawn_attacks(Color::BLACK, sq);
        arrReverseSquareBit[sq] = reverse_bb(square_bb(sq));
    }

#if defined(USE_PEXT)
    init_pext_attacks();
#endif
}

// In the PEXT build the sliding functions are inline in attacks.h.
#if !defined(USE_PEXT)

Bitboard bishop_attacks(Bitboard occ, Square sq)
{
    return hq_bishop_attacks(occ, sq);
}

Bitboard rook_attacks(Bitboard occ, Square sq)
{
    return hq_rook_attacks(occ, sq);
}

Bitboard queen_attacks(Bitboard occ, Square sq)
{
    return bishop_attacks(occ, sq) | rook_attacks(occ, sq);
}

#endif