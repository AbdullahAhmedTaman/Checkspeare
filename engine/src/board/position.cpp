#include "position.h"
#include "bitboard.h"
#include "evaluate.h"
#include "pst.h"
#include <cassert>

/*void Position::update_occupancy()
{
    occupancy[WHITE] = pieces[WHITE][PAWN] | pieces[WHITE][KNIGHT] | pieces[WHITE][BISHOP] | pieces[WHITE][ROOK] |
                       pieces[WHITE][QUEEN] | pieces[WHITE][KING];

    occupancy[BLACK] = pieces[BLACK][PAWN] | pieces[BLACK][KNIGHT] | pieces[BLACK][BISHOP] | pieces[BLACK][ROOK] |
                       pieces[BLACK][QUEEN] | pieces[BLACK][KING];

    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];
}*/

namespace
{
    inline void move_piece_eval(Position& pos, PieceType pt, Color c, Square from, Square to)
    {
        pos.mg_score[c] -= Eval::MG_WEIGHT[pt] + Eval::PST.mg[c][pt][from];
        pos.eg_score[c] -= Eval::EG_WEIGHT[pt] + Eval::PST.eg[c][pt][from];

        pos.mg_score[c] += Eval::MG_WEIGHT[pt] + Eval::PST.mg[c][pt][to];
        pos.eg_score[c] += Eval::EG_WEIGHT[pt] + Eval::PST.eg[c][pt][to];
    }

    inline void remove_piece_eval(Position& pos, PieceType pt, Color c, Square sq)
    {
        pos.mg_score[c] -= Eval::MG_WEIGHT[pt] + Eval::PST.mg[c][pt][sq];
        pos.eg_score[c] -= Eval::EG_WEIGHT[pt] + Eval::PST.eg[c][pt][sq];
        pos.game_phase -= Eval::PHASE_WEIGHTS[pt];
    }

    inline void add_piece_eval(Position& pos, PieceType pt, Color c, Square sq)
    {
        pos.mg_score[c] += Eval::MG_WEIGHT[pt] + Eval::PST.mg[c][pt][sq];
        pos.eg_score[c] += Eval::EG_WEIGHT[pt] + Eval::PST.eg[c][pt][sq];
        pos.game_phase += Eval::PHASE_WEIGHTS[pt];
    }
} // namespace

void Position::make_move(Move m)
{
    Square from = m.from();
    Square to = m.to();
    int flag = m.flag();

    Piece moving_piece = board[from];
    PieceType pt = type_of(moving_piece);
    Piece captured_piece = board[to];

    // 1. save History
    history[ply].castlingRights = castlingRights;
    history[ply].enPassantSquare = enPassantSquare;
    history[ply].halfmoveClock = halfmoveClock;
    history[ply].capturedPiece = captured_piece;
    history[ply].key = key;
    history[ply].mg_score[WHITE] = mg_score[WHITE];
    history[ply].mg_score[BLACK] = mg_score[BLACK];
    history[ply].eg_score[WHITE] = eg_score[WHITE];
    history[ply].eg_score[BLACK] = eg_score[BLACK];
    history[ply].game_phase = game_phase;
    ply++;

    // 2. Instead of clearing from and setting to separately, we create a XOR mask for the moving piece
    Bitboard move_mask = square_bb(from) | square_bb(to);

    // 3. Handle Promotions vs. Standard Moves
    if (flag >= KNIGHT_PROMO && flag <= QUEEN_PROMO_CAP)
    {
        PieceType promo_pt = (flag == KNIGHT_PROMO || flag == KNIGHT_PROMO_CAP)   ? KNIGHT
                             : (flag == BISHOP_PROMO || flag == BISHOP_PROMO_CAP) ? BISHOP
                             : (flag == ROOK_PROMO || flag == ROOK_PROMO_CAP)     ? ROOK
                                                                                  : QUEEN;

        pieces[sideToMove][PAWN] ^= square_bb(from);   // Erase pawn 1^1 = 0
        pieces[sideToMove][promo_pt] ^= square_bb(to); // Spawn promoted piece
        board[to] = static_cast<Piece>((sideToMove == WHITE) ? promo_pt : promo_pt + 6);
        remove_piece_eval(*this, PAWN, sideToMove, from);
        add_piece_eval(*this, promo_pt, sideToMove, to);
    }
    else
    {
        pieces[sideToMove][pt] ^= move_mask; // Move piece instantly
        board[to] = moving_piece;
        move_piece_eval(*this, pt, sideToMove, from, to);
    }

    // Update occupancy for the moving piece (Always shifts from -> to)
    occupancy[sideToMove] ^= move_mask;
    occupancy[BOTH] ^= move_mask;
    board[from] = PIECE_NONE;

    // 4. Handle Standard Captures
    if (captured_piece != PIECE_NONE)
    {
        PieceType captured_pt = type_of(captured_piece);
        Bitboard capture_mask = square_bb(to);

        pieces[sideToMove ^ 1][captured_pt] ^= capture_mask;
        occupancy[sideToMove ^ 1] ^= capture_mask; // Remove captured piece from opponent's occupancy
        occupancy[BOTH] ^= capture_mask; // Flips destination from 0 back to 1 (1^1=0, 0^1=1), keeping it occupied.
        remove_piece_eval(*this, captured_pt, static_cast<Color>(sideToMove ^ 1), to);
    }

    // 5. Handle Special Flags (En Passant & Castling)
    if (flag == EP_CAPTURE)
    {
        Square ep_pawn_sq = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        Bitboard ep_mask = square_bb(ep_pawn_sq);

        pieces[sideToMove ^ 1][PAWN] ^= ep_mask;
        occupancy[sideToMove ^ 1] ^= ep_mask;
        occupancy[BOTH] ^= ep_mask;
        board[ep_pawn_sq] = PIECE_NONE;
        remove_piece_eval(*this, PAWN, static_cast<Color>(sideToMove ^ 1), ep_pawn_sq);
    }
    else if (flag == KING_CASTLE || flag == QUEEN_CASTLE)
    {
        Square rook_from =
            (flag == KING_CASTLE) ? ((sideToMove == WHITE) ? H1 : H8) : ((sideToMove == WHITE) ? A1 : A8);
        Square rook_to = (flag == KING_CASTLE) ? ((sideToMove == WHITE) ? F1 : F8) : ((sideToMove == WHITE) ? D1 : D8);

        Bitboard rook_mask = square_bb(rook_from) | square_bb(rook_to);

        pieces[sideToMove][ROOK] ^= rook_mask;
        occupancy[sideToMove] ^= rook_mask;
        occupancy[BOTH] ^= rook_mask;

        board[rook_from] = PIECE_NONE;
        board[rook_to] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
        move_piece_eval(*this, ROOK, sideToMove, rook_from, rook_to);
    }

    // 6. Update Clocks and State
    enPassantSquare = SQ_NONE;
    if (flag == DOUBLE_PAWN_PUSH)
    {
        enPassantSquare = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
    }

    halfmoveClock++;
    if (pt == PAWN || captured_piece != PIECE_NONE)
        halfmoveClock = 0;

    sideToMove = static_cast<Color>(sideToMove ^ 1);
}

void Position::unmake_move(Move m)
{
    sideToMove = static_cast<Color>(sideToMove ^ 1);
    ply--;

    Square from = m.from();
    Square to = m.to();
    int flag = m.flag();

    Piece moved_piece = board[to];
    PieceType pt = type_of(moved_piece);

    // 1. Restore History
    castlingRights = history[ply].castlingRights;
    enPassantSquare = history[ply].enPassantSquare;
    halfmoveClock = history[ply].halfmoveClock;
    key = history[ply].key;
    Piece captured_piece = history[ply].capturedPiece;
    mg_score[WHITE] = history[ply].mg_score[WHITE];
    mg_score[BLACK] = history[ply].mg_score[BLACK];
    eg_score[WHITE] = history[ply].eg_score[WHITE];
    eg_score[BLACK] = history[ply].eg_score[BLACK];
    game_phase = history[ply].game_phase;

    Bitboard move_mask = square_bb(from) | square_bb(to);

    // 2. Undo Moving Piece & Promotions
    if (flag >= KNIGHT_PROMO && flag <= QUEEN_PROMO_CAP)
    {
        pieces[sideToMove][pt] ^= square_bb(to);     // Erase promoted piece
        pieces[sideToMove][PAWN] ^= square_bb(from); // Restore pawn
        board[from] = static_cast<Piece>((sideToMove == WHITE) ? WHITE_PAWN : BLACK_PAWN);
    }
    else
    {
        pieces[sideToMove][pt] ^= move_mask;
        board[from] = static_cast<Piece>((sideToMove == WHITE) ? pt : pt + 6);
    }

    occupancy[sideToMove] ^= move_mask;
    occupancy[BOTH] ^= move_mask;
    board[to] = PIECE_NONE; // Clear it initially

    // 3. Undo Standard Captures
    if (captured_piece != PIECE_NONE && flag != EP_CAPTURE)
    {
        PieceType captured_pt = type_of(captured_piece);
        Bitboard capture_mask = square_bb(to);

        pieces[sideToMove ^ 1][captured_pt] ^= capture_mask;
        occupancy[sideToMove ^ 1] ^= capture_mask;
        occupancy[BOTH] ^= capture_mask;
        board[to] = captured_piece;
    }

    // 4. Undo Special Moves
    if (flag == EP_CAPTURE)
    {
        Square ep_pawn_sq = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        Bitboard ep_mask = square_bb(ep_pawn_sq);

        pieces[sideToMove ^ 1][PAWN] ^= ep_mask;
        occupancy[sideToMove ^ 1] ^= ep_mask;
        occupancy[BOTH] ^= ep_mask;
        board[ep_pawn_sq] = (sideToMove == WHITE) ? BLACK_PAWN : WHITE_PAWN;
    }
    else if (flag == KING_CASTLE || flag == QUEEN_CASTLE)
    {
        Square rook_from =
            (flag == KING_CASTLE) ? ((sideToMove == WHITE) ? H1 : H8) : ((sideToMove == WHITE) ? A1 : A8);
        Square rook_to = (flag == KING_CASTLE) ? ((sideToMove == WHITE) ? F1 : F8) : ((sideToMove == WHITE) ? D1 : D8);

        Bitboard rook_mask = square_bb(rook_from) | square_bb(rook_to);

        pieces[sideToMove][ROOK] ^= rook_mask;
        occupancy[sideToMove] ^= rook_mask;
        occupancy[BOTH] ^= rook_mask;

        board[rook_to] = PIECE_NONE;
        board[rook_from] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
    }
}

bool Position::is_square_attacked(Square sq, Color attackingColor) const
{
    const Bitboard occupied =
        occupancy[BOTH] | square_bb(sq); // OR-ing is necessary because we're using HQ method for sliding piece attacks.

    if (pawn_attacks(static_cast<Color>(attackingColor ^ 1), sq) & pieces[attackingColor][PAWN])
        return true;

    if (knight_attacks(sq) & pieces[attackingColor][KNIGHT])
        return true;

    if (king_attacks(sq) & pieces[attackingColor][KING])
        return true;

    if (bishop_attacks(occupied, sq) & (pieces[attackingColor][BISHOP] | pieces[attackingColor][QUEEN]))
        return true;

    return rook_attacks(occupied, sq) & (pieces[attackingColor][ROOK] | pieces[attackingColor][QUEEN]);
}