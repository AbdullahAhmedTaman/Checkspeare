#pragma once
#include "types.h"
#include <string>

struct UndoState
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

constexpr int MAX_GAME_PLY = 1024;

struct Position
{
    Bitboard pieces[2][PieceType::NONE];
    Bitboard occupancy[3];
    Piece board[Square::SQ_NONE];
    Color sideToMove;
    uint8_t castlingRights;
    Square enPassantSquare;
    uint16_t halfmoveClock;
    Key key;
    int mg_score[2] = {0, 0};
    int eg_score[2] = {0, 0};
    int game_phase = 0;

    UndoState history[MAX_GAME_PLY];
    int ply = 0;

    // Resets to an empty board, White to move, zero clocks/key/eval/ply.
    // board[] is explicitly filled with PIECE_NONE.
    void clear() noexcept;

    // Construct a position from a standard FEN string
    static Position from_fen(const std::string& fen);
    [[nodiscard]] std::string to_fen() const;

    Position() noexcept;

    void make_move(Move m);
    void unmake_move(Move m);
    void update_occupancy(); // Unused for now
    bool is_square_attacked(Square sq, Color attackingColor) const;
    Square king_square(Color c) const;
    [[nodiscard]] bool in_check() const;
    [[nodiscard]] bool is_repetition() const;
};