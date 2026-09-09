#pragma once
#include "attacks.h"
#include "types.h"

/**
 * @brief Stores the irreversible state of the board before a move is made.
 *
 * To optimize the search tree traversal, the engine saves only the variables that cannot be
 * mathematically reversed.
 */
struct UndoState // Edit <<<=====
{
    uint8_t castlingRights;
    Square enPassantSquare;
    uint16_t halfmoveClock;
    Piece capturedPiece;
    Key key;
    int mg_score[2] = {0, 0};
    int eg_score[2] = {0, 0};
    int game_phase = 0;
};

/** @brief Maximum allowed plies (half-moves) in a game or search depth. */
constexpr int MAX_GAME_PLY = 1024;

struct Position
{
    Bitboard pieces[2][PieceType::NONE]; //[color][pieceType]
    Bitboard occupancy[3];
    Piece board[Square::SQ_NONE];
    Color sideToMove;
    uint8_t castlingRights;
    Square enPassantSquare;
    uint16_t halfmoveClock;
    Key key;                  // Zobrist hash
    int mg_score[2] = {0, 0}; // Middlegame score for [WHITE] and [BLACK]
    int eg_score[2] = {0, 0}; // Endgame score for [WHITE] and [BLACK]
    int game_phase = 0;       // Tracks transition from middlegame to endgame

    UndoState history[MAX_GAME_PLY]; ///< Stack array storing irreversible state history.
    int ply = 0;                     ///< Current depth index in the history stack.

    /**
     * @brief Executes a physical move on the board and updates state.
     * @param m The encoded move to execute.
     */
    void make_move(Move m);

    /**
     * @brief Reverses the last executed move to exactly restore the prior board state.
     * @param m The exact move that was just played.
     */
    void unmake_move(Move m);

    /**
     * @brief Recomputes the White, Black, and Both occupancy bitboards.
     */
    void update_occupancy();

    /**
     * @brief Checks whether a square is attacked by a given color.
     * @param sq Target square.
     * @param byColor Color of the attacking side.
     */
    bool is_square_attacked(Square sq, Color attackingColor) const;
};
