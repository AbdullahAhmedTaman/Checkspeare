#include "zobrist.h"
#include <cstdint>
#include <random>

Key piece_keys[12][64];
Key side_key;
Key castle_keys[16];
Key en_passant_keys[8];

namespace zobrist
{
    /**
     * @brief Initialize the Zobrist hashing keys.
     *
     */
    void init()
    {
        std::mt19937_64 rng(1070372ULL); /// Seed the PRNG with a fixed value for deterministic behavior.

        // Generate random keys for each piece type and square combination.
        for (int piece = 0; piece < 12; ++piece)
        {
            for (int square = 0; square < 64; ++square)
            {
                piece_keys[piece][square] = rng();
            }
        }
        side_key = rng(); // Random key for the side to move.
        // Generate random keys for each castling right combination.
        for (int i = 0; i < 16; ++i)
        {
            castle_keys[i] = rng();
        }
        // Generate random keys for each en passant file (a-h).
        for (int i = 0; i < 8; ++i)
        {
            en_passant_keys[i] = rng();
        }
    }

} // namespace zobrist