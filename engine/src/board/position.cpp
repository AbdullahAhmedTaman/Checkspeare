#include "position.h"
#include "bitboard.h"
#include <cassert>

void Position::update_occupancy()
{
    occupancy[WHITE] = BB_EMPTY;
    occupancy[BLACK] = BB_EMPTY;

    for (int pt = PAWN; pt < NONE; ++pt)
    {
        occupancy[WHITE] |= pieces[WHITE][pt];
        occupancy[BLACK] |= pieces[BLACK][pt];
    }
    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];
}

void Position::make_move(Move m)
{
    Square from = m.from();
    Square to = m.to();
    int flag = m.flag();

    Piece moving_piece = board[from];
    PieceType pt = static_cast<PieceType>(moving_piece % 6); // normalizes piece colors down to base piece types.
    Piece captured_piece = board[to];

    // 1. Save irreversible state to the history stack
    history[ply].castlingRights = castlingRights;
    history[ply].enPassantSquare = enPassantSquare;
    history[ply].halfmoveClock = halfmoveClock;
    history[ply].capturedPiece = captured_piece;
    history[ply].key = key;
    ply++;

    // 2.Clear the moving piece from its square
    clear_bit(pieces[sideToMove][pt], from);
    board[from] = PIECE_NONE;

    // 3. Handle standard captures by removing enemy piece
    if (captured_piece != PIECE_NONE)
    {
        PieceType captured_pt = static_cast<PieceType>(captured_piece % 6);
        clear_bit(pieces[sideToMove ^ 1][captured_pt], to);
    }

    // 4. Process special move flags
    if (flag == EP_CAPTURE) // en passant capture
    {
        Square ep_pawn_sq = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        clear_bit(pieces[sideToMove ^ 1][PAWN], ep_pawn_sq);
        board[ep_pawn_sq] = PIECE_NONE;
    }
    // We do not set board[to] = PIECE_NONE;
    // because that square is immediately overwritten by the attacking piece(King) in Step 5
    else if (flag == KING_CASTLE)
    {
        Square rook_from = (sideToMove == WHITE) ? H1 : H8;
        Square rook_to = (sideToMove == WHITE) ? F1 : F8;
        clear_bit(pieces[sideToMove][ROOK], rook_from);
        set_bit(pieces[sideToMove][ROOK], rook_to);
        board[rook_from] = PIECE_NONE;
        board[rook_to] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
    }
    else if (flag == QUEEN_CASTLE)
    {
        Square rook_from = (sideToMove == WHITE) ? A1 : A8;
        Square rook_to = (sideToMove == WHITE) ? D1 : D8;
        clear_bit(pieces[sideToMove][ROOK], rook_from);
        set_bit(pieces[sideToMove][ROOK], rook_to);
        board[rook_from] = PIECE_NONE;
        board[rook_to] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
    }

    // 5. Place the moving piece and handle pawn promotions
    if (flag >= KNIGHT_PROMO && flag <= QUEEN_PROMO_CAP)
    {
        PieceType promo_pt;
        if (flag == KNIGHT_PROMO || flag == KNIGHT_PROMO_CAP)
            promo_pt = KNIGHT;
        else if (flag == BISHOP_PROMO || flag == BISHOP_PROMO_CAP)
            promo_pt = BISHOP;
        else if (flag == ROOK_PROMO || flag == ROOK_PROMO_CAP)
            promo_pt = ROOK;
        else
            promo_pt = QUEEN;

        set_bit(pieces[sideToMove][promo_pt], to);
        board[to] = static_cast<Piece>((sideToMove == WHITE) ? promo_pt : promo_pt + 6);
    }
    else
    {
        set_bit(pieces[sideToMove][pt], to);
        board[to] = moving_piece;
    }

    // 6. Update rule-based clocks and en passant targets
    enPassantSquare = SQ_NONE;
    if (flag == DOUBLE_PAWN_PUSH)
    {
        enPassantSquare = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
    }

    halfmoveClock++;
    if (pt == PAWN || captured_piece != PIECE_NONE)
    {
        halfmoveClock = 0;
    }

    // 7. Toggle turn and recalculate sliding blockers
    sideToMove = static_cast<Color>(sideToMove ^ 1);
    update_occupancy();
}

void Position::unmake_move(Move m)
{
    // 1. Revert turn and decrement ply index(to access the last saved state in the history stack)
    sideToMove = static_cast<Color>(sideToMove ^ 1);
    ply--;
    assert(ply >= 0);

    Square from = m.from();
    Square to = m.to();
    int flag = m.flag();

    Piece moved_piece = board[to];
    PieceType pt = static_cast<PieceType>(moved_piece % 6);

    // 2. Restore all irreversible state variables from the history stack
    castlingRights = history[ply].castlingRights;
    enPassantSquare = history[ply].enPassantSquare;
    halfmoveClock = history[ply].halfmoveClock;
    key = history[ply].key;
    Piece captured_piece = history[ply].capturedPiece;

    // 3. Clear the destination square, reverting promotions
    if (flag >= KNIGHT_PROMO && flag <= QUEEN_PROMO_CAP)
    {
        clear_bit(pieces[sideToMove][pt], to);
        pt = PAWN;
    }
    else
    {
        clear_bit(pieces[sideToMove][pt], to);
    }

    // 4. Return the moving piece to its origin square
    set_bit(pieces[sideToMove][pt], from);
    board[from] = static_cast<Piece>((sideToMove == WHITE) ? pt : pt + 6);
    board[to] = PIECE_NONE;

    // 5. Restore standard captured pieces
    if (captured_piece != PIECE_NONE && flag != EP_CAPTURE)
    {
        PieceType captured_pt = static_cast<PieceType>(captured_piece % 6);
        set_bit(pieces[sideToMove ^ 1][captured_pt], to);
        board[to] = captured_piece;
    }

    // 6. Revert special board geometry changes (En Passant & Castling)
    if (flag == EP_CAPTURE)
    {
        Square ep_pawn_sq = (sideToMove == WHITE) ? static_cast<Square>(to - 8) : static_cast<Square>(to + 8);
        set_bit(pieces[sideToMove ^ 1][PAWN], ep_pawn_sq);
        board[ep_pawn_sq] = (sideToMove == WHITE) ? BLACK_PAWN : WHITE_PAWN;
    }
    else if (flag == KING_CASTLE)
    {
        Square rook_from = (sideToMove == WHITE) ? H1 : H8;
        Square rook_to = (sideToMove == WHITE) ? F1 : F8;
        clear_bit(pieces[sideToMove][ROOK], rook_to);
        set_bit(pieces[sideToMove][ROOK], rook_from);
        board[rook_to] = PIECE_NONE;
        board[rook_from] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
    }
    else if (flag == QUEEN_CASTLE)
    {
        Square rook_from = (sideToMove == WHITE) ? A1 : A8;
        Square rook_to = (sideToMove == WHITE) ? D1 : D8;
        clear_bit(pieces[sideToMove][ROOK], rook_to);
        set_bit(pieces[sideToMove][ROOK], rook_from);
        board[rook_to] = PIECE_NONE;
        board[rook_from] = (sideToMove == WHITE) ? WHITE_ROOK : BLACK_ROOK;
    }

    // 7. Resynchronize occupancy bitboards
    update_occupancy();
}