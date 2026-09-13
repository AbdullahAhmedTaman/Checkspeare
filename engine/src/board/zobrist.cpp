#include "zobrist.h"
#include "bitboard.h"
#include "position.h"
#include <cassert>
#include <random>

Key piece_keys[12][64];
Key side_key;
Key castle_keys[16];
Key en_passant_keys[8];

namespace Zobrist
{
    void init()
    {
        std::mt19937_64 rng(1070372ULL); // fixed seed

        for (int piece = 0; piece < 12; ++piece)
            for (int square = 0; square < 64; ++square)
                piece_keys[piece][square] = rng();

        side_key = rng();

        for (int i = 0; i < 16; ++i)
            castle_keys[i] = rng();

        for (int i = 0; i < 8; ++i)
            en_passant_keys[i] = rng();
    }

    Key generate_key(const Position& pos)
    {
        Key final_key = 0ULL;

        Bitboard occupied_squares = pos.occupancy[BOTH];
        while (occupied_squares)
        {
            const Square square = pop_lsb(occupied_squares);
            const Piece piece = pos.board[square];
            assert(piece != PIECE_NONE && "board/occupancy out of sync");
            final_key ^= piece_keys[static_cast<int>(piece)][square];
        }

        if (pos.sideToMove == BLACK)
            final_key ^= side_key;

        final_key ^= castle_keys[pos.castlingRights];

        // EP keys are indexed by file only: the EP square is always on rank 3
        // or 6, so the rank is implied by the side to move.
        if (pos.enPassantSquare != SQ_NONE)
            final_key ^= en_passant_keys[file_of(pos.enPassantSquare)];

        return final_key;
    }
} // namespace Zobrist