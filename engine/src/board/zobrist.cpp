#include "zobrist.h"
#include "types.h"
#include <cstdint>
#include <random>

Key piece_keys[12][64];
Key side_key;
Key castle_keys[16];
Key en_passant_keys[8];

namespace Zobrist
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

    /**
     * @brief Generate a Zobrist hash key for the given position.
     *
     * @param pos The position to generate the key for.
     * @return The generated Zobrist hash key.
     */
    Key generate_key(const Position& pos)
    {
        Key final_key = 0ULL;
        // XOR the keys for each piece on the board.
        Bitboard occupied_squares = pos.occupancy[2]; // Get the bitboard of all occupied squares. (Note: Consider using
                                                      // a named constant like BOTH instead of 2 for better clarity).

        // Generate the Zobrist key by XORing the keys for each piece on the board.
        while (occupied_squares)
        {
            int square = __builtin_ctzll(occupied_squares); // Get the index of the least significant bit (LSB).
            Piece piece = pos.board[square];                // Mailbox lookup for the piece at this square.
            final_key ^=
                piece_keys[static_cast<int>(piece)]
                          [square]; // XOR the piece key into the final hash(cast the piece to an int for indexing).
            occupied_squares &=
                occupied_squares - 1; // Clear the lowest set bit (LSB) to move to the next occupied square.
        }

        if (pos.sideToMove == BLACK)
        {
            final_key ^= side_key;
        }

        final_key ^= castle_keys[pos.castlingRights]; // XOR the key for the current castling rights.
        if (pos.enPassantSquare != SQ_NONE)
        {
            int file = pos.enPassantSquare & 7; // Get the file of the en passant square (0-7). (Optimized: Using fast
                                                // bitwise AND instead of modulo %8).
            final_key ^= en_passant_keys[file]; // XOR the key for the en passant file.
        }
        return final_key;
    }
} // namespace Zobrist