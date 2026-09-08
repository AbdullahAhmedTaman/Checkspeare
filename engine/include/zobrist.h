#pragma once
#define ZOBRIST_H

#include "position.h"
#include "types.h"
#include <cstdint>
/**
 * @brief Global Zobrist random keys used to hash board states into 64-bit integers.
 *
 * Each piece type and square combination has a unique random key, as do the side to move, castling rights, and en
 * passant files.
 */

extern Key piece_keys[12][64]; ///< Random numbers for each [piece_type][square] combination.
extern Key side_key;           ///< Random number XORed when it is Black's turn to move.
extern Key castle_keys[16];    ///< Random numbers for each castling right combination.
extern Key en_passant_keys[8]; ///< Random numbers for each en passant file(a-h).

namespace Zobrist
{
    /**
     * @brief Seeds the PRNG and populates all global key arrays with deterministic random numbers.
     *
     * Must be called during engine initialization before any board hashing occurs.
     */
    void init();
    /**
     * @brief Generates a unique 64-bit hash key for the given board position.
     *
     * The key is computed by XORing the random numbers corresponding to each piece on the board, the side to move,
     * castling rights, and en passant file (if applicable).
     *
     * @param pos The current board position to evaluate.
     * @return A 64-bit integer representing the unique hash of the position.
     */
    uint64_t generate_key(const Position& pos);
} // namespace Zobrist
