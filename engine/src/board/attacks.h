#pragma once

#include "types.h"

#include <cstdint>

#if defined(ATTACKS_ROOK_COMPRESSED) && !defined(USE_PEXT)
#error "ATTACKS_ROOK_COMPRESSED requires USE_PEXT"
#endif

#if defined(USE_PEXT)
#if !(defined(__x86_64__) || defined(_M_X64)) || defined(_M_ARM64EC)
#error "USE_PEXT requires a 64-bit x86 target (_pext_u64 is unavailable on ARM and 32-bit x86)"
#endif
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__BMI2__)
#error "USE_PEXT: compile with -mbmi2 (or -march=x86-64-v3 / haswell / native)"
#endif
#include <immintrin.h>
#endif

// Initialization
void init_attacks(); // Must be called once at the start of the engine

// Tables owned by attacks.cpp. They are declared here only so the inline
// functions below can read them; nothing outside attacks.* should touch them.
namespace attacks_detail
{
    extern Bitboard arrKnight[Square::SQ_NONE];
    extern Bitboard arrKing[Square::SQ_NONE];
    extern Bitboard arrPawn[2][Square::SQ_NONE]; // [color][square]

#if defined(USE_PEXT)
    struct alignas(16) PextEntry
    {
        Bitboard mask;   // relevant occupancy squares for this piece and square
        Bitboard* table; // 2^popcount(mask) attack sets
    };

    extern PextEntry bishopEntries[64];

#if defined(ATTACKS_ROOK_COMPRESSED)
    struct alignas(32) RookEntry
    {
        Bitboard mask;        // relevant occupancy squares
        Bitboard rays;        // empty-board rook attacks from this square
        std::uint16_t* table; // attack sets packed into <= 14 bits
    };

    extern RookEntry rookEntries[64];
#else
    extern PextEntry rookEntries[64];
#endif
#endif
} // namespace attacks_detail

// Non-sliding pieces: plain table reads, inlined into the callers.
inline Bitboard pawn_attacks(Color color, Square sq)
{
    return attacks_detail::arrPawn[static_cast<int>(color)][sq];
}

inline Bitboard knight_attacks(Square sq)
{
    return attacks_detail::arrKnight[sq];
}

inline Bitboard king_attacks(Square sq)
{
    return attacks_detail::arrKing[sq];
}

// Sliding pieces
#if defined(USE_PEXT)

inline Bitboard bishop_attacks(Bitboard occupancy, Square sq)
{
    const attacks_detail::PextEntry& e = attacks_detail::bishopEntries[sq];
    return e.table[_pext_u64(occupancy, e.mask)];
}

#if defined(ATTACKS_ROOK_COMPRESSED)

// 16-bit table entry -> expand into the real attack squares with PDEP.
inline Bitboard rook_attacks(Bitboard occupancy, Square sq)
{
    const attacks_detail::RookEntry& e = attacks_detail::rookEntries[sq];
    return _pdep_u64(e.table[_pext_u64(occupancy, e.mask)], e.rays);
}

#else

inline Bitboard rook_attacks(Bitboard occupancy, Square sq)
{
    const attacks_detail::PextEntry& e = attacks_detail::rookEntries[sq];
    return e.table[_pext_u64(occupancy, e.mask)];
}

#endif

inline Bitboard queen_attacks(Bitboard occupancy, Square sq)
{
    return bishop_attacks(occupancy, sq) | rook_attacks(occupancy, sq);
}

#else // HQ build: defined out-of-line in attacks.cpp

Bitboard bishop_attacks(Bitboard occupancy, Square sq);
Bitboard rook_attacks(Bitboard occupancy, Square sq);
Bitboard queen_attacks(Bitboard occupancy, Square sq);

#endif