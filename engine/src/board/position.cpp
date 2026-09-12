#include "position.h"
#include "bitboard.h"
#include "evaluate.h"
#include "pst.h"
#include "zobrist.h"
#include <cassert>
#include <cctype>
#include <sstream>


namespace
{
    // Evaluation updates.
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

    // History stack.
    inline void push_state(Position& pos, Piece captured)
    {
        UndoState& h = pos.history[pos.ply];
        h.castlingRights = pos.castlingRights;
        h.enPassantSquare = pos.enPassantSquare;
        h.halfmoveClock = pos.halfmoveClock;
        h.capturedPiece = captured;
        h.key = pos.key;
        h.mg_score[WHITE] = pos.mg_score[WHITE];
        h.mg_score[BLACK] = pos.mg_score[BLACK];
        h.eg_score[WHITE] = pos.eg_score[WHITE];
        h.eg_score[BLACK] = pos.eg_score[BLACK];
        h.game_phase = pos.game_phase;
        ++pos.ply;
    }

    // Restores the state saved by the matching push_state.
    inline Piece pop_state(Position& pos)
    {
        --pos.ply;
        const UndoState& h = pos.history[pos.ply];
        pos.castlingRights = h.castlingRights;
        pos.enPassantSquare = h.enPassantSquare;
        pos.halfmoveClock = h.halfmoveClock;
        pos.key = h.key;
        pos.mg_score[WHITE] = h.mg_score[WHITE];
        pos.mg_score[BLACK] = h.mg_score[BLACK];
        pos.eg_score[WHITE] = h.eg_score[WHITE];
        pos.eg_score[BLACK] = h.eg_score[BLACK];
        pos.game_phase = h.game_phase;
        return h.capturedPiece;
    }

    // Applied as castlingRights &= MASK[from] & MASK[to]. Indexed by square,
    // not by piece: touching a corner or king square clears the corresponding
    // right regardless of which piece moved there.
    // clang-format off
    constexpr uint8_t CASTLING_RIGHTS_MASK[64] = {
        13, 15, 15, 15, 12, 15, 15, 14, // A1, E1, H1
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        7,  15, 15, 15, 3,  15, 15, 11 // A8, E8, H8
    };
    // clang-format on

} // namespace

Position::Position() noexcept
{
    clear();
}

void Position::clear() noexcept
{
    for (int c = 0; c < 2; ++c)
        for (int pt = 0; pt < PieceType::NONE; ++pt)
            pieces[c][pt] = 0;

    for (int i = 0; i < 3; ++i)
        occupancy[i] = 0;

    for (int s = 0; s < Square::SQ_NONE; ++s)
        board[s] = PIECE_NONE;

    sideToMove = WHITE;
    castlingRights = 0;
    enPassantSquare = SQ_NONE;
    halfmoveClock = 0;
    key = 0;
    mg_score[WHITE] = mg_score[BLACK] = 0;
    eg_score[WHITE] = eg_score[BLACK] = 0;
    game_phase = 0;
    ply = 0;
    // history[] left untouched: entries are always written before read.
}

void Position::update_occupancy() // Unused for now
{
    occupancy[WHITE] = pieces[WHITE][PAWN] | pieces[WHITE][KNIGHT] | pieces[WHITE][BISHOP] | pieces[WHITE][ROOK] |
                       pieces[WHITE][QUEEN] | pieces[WHITE][KING];

    occupancy[BLACK] = pieces[BLACK][PAWN] | pieces[BLACK][KNIGHT] | pieces[BLACK][BISHOP] | pieces[BLACK][ROOK] |
                       pieces[BLACK][QUEEN] | pieces[BLACK][KING];

    occupancy[BOTH] = occupancy[WHITE] | occupancy[BLACK];
}

void Position::make_move(Move m)
{
    assert(m.is_ok() && "make_move called with none()/invalid move");
    assert(ply >= 0 && ply < MAX_GAME_PLY && "history stack overflow");

    if (m.is_null())
    {
        push_state(*this, PIECE_NONE);

        // Clear the EP file from the key.
        if (enPassantSquare != SQ_NONE)
            key ^= en_passant_keys[file_of(enPassantSquare)];

        enPassantSquare = SQ_NONE;
        ++halfmoveClock;
        sideToMove = static_cast<Color>(sideToMove ^ 1);
        key ^= side_key;
        return;
    }

    assert(is_valid(m.from()) && is_valid(m.to()));
    assert(board[m.from()] != PIECE_NONE && "make_move on an empty from-square");

    const Square from = m.from();
    const Square to = m.to();

    const Piece moving_piece = board[from];
    const PieceType pt = type_of(moving_piece);
    const Piece captured_piece = board[to];

    const Color us = sideToMove;
    const Color them = static_cast<Color>(us ^ 1);

    push_state(*this, captured_piece);

    // Drop the previous en passant file from the key.
    if (enPassantSquare != SQ_NONE)
        key ^= en_passant_keys[file_of(enPassantSquare)];

    // move_mask moves from -> to for non-promotions, since from != to.
    const Bitboard move_mask = square_bb(from) | square_bb(to);

    if (m.is_promotion())
    {
        const PieceType promo_pt = promotion_type(m);
        const Piece promo_piece = make_piece(us, promo_pt);

        pieces[us][PAWN] ^= square_bb(from);
        pieces[us][promo_pt] ^= square_bb(to);
        board[to] = promo_piece;

        key ^= piece_keys[static_cast<int>(moving_piece)][from];
        key ^= piece_keys[static_cast<int>(promo_piece)][to];

        remove_piece_eval(*this, PAWN, us, from);
        add_piece_eval(*this, promo_pt, us, to);
    }
    else
    {
        pieces[us][pt] ^= move_mask;
        board[to] = moving_piece;

        key ^= piece_keys[static_cast<int>(moving_piece)][from];
        key ^= piece_keys[static_cast<int>(moving_piece)][to];

        move_piece_eval(*this, pt, us, from, to);
    }

    occupancy[us] ^= move_mask;
    occupancy[BOTH] ^= move_mask;
    board[from] = PIECE_NONE;

    // Standard captures. EP captures leave board[to] empty and fall through
    // to the special-move block below.
    if (captured_piece != PIECE_NONE)
    {
        const PieceType captured_pt = type_of(captured_piece);
        const Bitboard capture_mask = square_bb(to);

        pieces[them][captured_pt] ^= capture_mask;
        occupancy[them] ^= capture_mask;
        occupancy[BOTH] ^= capture_mask;

        key ^= piece_keys[static_cast<int>(captured_piece)][to];

        remove_piece_eval(*this, captured_pt, them, to);
    }

    if (m.is_ep())
    {
        const Square ep_pawn_sq = square_behind(to, us);
        const Bitboard ep_mask = square_bb(ep_pawn_sq);
        const Piece ep_pawn = make_piece(them, PAWN);

        pieces[them][PAWN] ^= ep_mask;
        occupancy[them] ^= ep_mask;
        occupancy[BOTH] ^= ep_mask;
        board[ep_pawn_sq] = PIECE_NONE;

        key ^= piece_keys[static_cast<int>(ep_pawn)][ep_pawn_sq];

        remove_piece_eval(*this, PAWN, them, ep_pawn_sq);
    }
    else if (m.is_castle())
    {
        const bool kingside = m.is_kingside_castle();
        const Square rook_from = kingside ? (us == WHITE ? H1 : H8) : (us == WHITE ? A1 : A8);
        const Square rook_to = kingside ? (us == WHITE ? F1 : F8) : (us == WHITE ? D1 : D8);

        const Bitboard rook_mask = square_bb(rook_from) | square_bb(rook_to);
        const Piece rook_piece = make_piece(us, ROOK);

        pieces[us][ROOK] ^= rook_mask;
        occupancy[us] ^= rook_mask;
        occupancy[BOTH] ^= rook_mask;

        board[rook_from] = PIECE_NONE;
        board[rook_to] = rook_piece;

        key ^= piece_keys[static_cast<int>(rook_piece)][rook_from];
        key ^= piece_keys[static_cast<int>(rook_piece)][rook_to];

        move_piece_eval(*this, ROOK, us, rook_from, rook_to);
    }

    const uint8_t old_castling = castlingRights;
    castlingRights &= CASTLING_RIGHTS_MASK[from] & CASTLING_RIGHTS_MASK[to];
    if (old_castling != castlingRights)
        key ^= castle_keys[old_castling] ^ castle_keys[castlingRights];

    enPassantSquare = SQ_NONE;
    if (m.is_double_push())
    {
        const Square candidate = square_behind(to, us);
        if (pawn_attacks(us, candidate) & pieces[them][PAWN])
        {
            enPassantSquare = candidate;
            key ^= en_passant_keys[file_of(candidate)];
        }
    }

    ++halfmoveClock;
    if (pt == PAWN || captured_piece != PIECE_NONE)
        halfmoveClock = 0;

    sideToMove = them;
    key ^= side_key;
}

void Position::unmake_move(Move m)
{
    assert(m.is_ok() && "unmake_move called with none()/invalid move");
    assert(ply > 0 && "unmake_move at ply 0");
    if (m.is_null())
    {
        sideToMove = static_cast<Color>(sideToMove ^ 1);
        pop_state(*this); // restores key, castling, EP, clocks, eval
        return;
    }

    assert(board[m.to()] != PIECE_NONE && "unmake_move on an empty to-square");
    sideToMove = static_cast<Color>(sideToMove ^ 1);

    const Square from = m.from();
    const Square to = m.to();

    const Piece moved_piece = board[to];
    const PieceType pt = type_of(moved_piece);

    const Color us = sideToMove;
    const Color them = static_cast<Color>(us ^ 1);

    // The key is restored directly from the stack rather than undone incrementally.
    const Piece captured_piece = pop_state(*this);

    const Bitboard move_mask = square_bb(from) | square_bb(to);

    if (m.is_promotion())
    {
        pieces[us][pt] ^= square_bb(to);     // promoted piece
        pieces[us][PAWN] ^= square_bb(from); // pawn
        board[from] = make_piece(us, PAWN);
    }
    else
    {
        pieces[us][pt] ^= move_mask;
        board[from] = make_piece(us, pt);
    }

    occupancy[us] ^= move_mask;
    occupancy[BOTH] ^= move_mask;
    board[to] = PIECE_NONE;

    if (captured_piece != PIECE_NONE && !m.is_ep())
    {
        const PieceType captured_pt = type_of(captured_piece);
        const Bitboard capture_mask = square_bb(to);

        pieces[them][captured_pt] ^= capture_mask;
        occupancy[them] ^= capture_mask;
        occupancy[BOTH] ^= capture_mask;
        board[to] = captured_piece;
    }

    if (m.is_ep())
    {
        const Square ep_pawn_sq = square_behind(to, us);
        const Bitboard ep_mask = square_bb(ep_pawn_sq);

        pieces[them][PAWN] ^= ep_mask;
        occupancy[them] ^= ep_mask;
        occupancy[BOTH] ^= ep_mask;
        board[ep_pawn_sq] = make_piece(them, PAWN);
    }
    else if (m.is_castle())
    {
        const bool kingside = m.is_kingside_castle();
        const Square rook_from = kingside ? (us == WHITE ? H1 : H8) : (us == WHITE ? A1 : A8);
        const Square rook_to = kingside ? (us == WHITE ? F1 : F8) : (us == WHITE ? D1 : D8);

        const Bitboard rook_mask = square_bb(rook_from) | square_bb(rook_to);

        pieces[us][ROOK] ^= rook_mask;
        occupancy[us] ^= rook_mask;
        occupancy[BOTH] ^= rook_mask;

        board[rook_to] = PIECE_NONE;
        board[rook_from] = make_piece(us, ROOK);
    }
}

bool Position::is_square_attacked(Square sq, Color attackingColor) const
{
    // sq is OR'd in because the sliding generators assume the target is occupied.
    const Bitboard occupied = occupancy[BOTH] | square_bb(sq);

    // A pawn of attackingColor attacks sq iff a pawn of the *opposite* color
    // on sq would attack back.
    if (pawn_attacks(static_cast<Color>(attackingColor ^ 1), sq) & pieces[attackingColor][PAWN])
        return true;

    if (knight_attacks(sq) & pieces[attackingColor][KNIGHT])
        return true;

    if (king_attacks(sq) & pieces[attackingColor][KING])
        return true;

    if (bishop_attacks(occupied, sq) & (pieces[attackingColor][BISHOP] | pieces[attackingColor][QUEEN]))
        return true;

    if (rook_attacks(occupied, sq) & (pieces[attackingColor][ROOK] | pieces[attackingColor][QUEEN]))
        return true;

    return false;
}

Square Position::king_square(Color c) const
{
    return lsb(pieces[c][KING]);
}

Position Position::from_fen(const std::string& fen)
{
    Position pos;
    pos.clear(); // Resets board to empty state[cite: 5, 6]

    std::istringstream ss(fen);
    std::string piece_placement, active_color, castling, en_passant;
    int halfmove = 0, fullmove = 1;

    ss >> piece_placement >> active_color >> castling >> en_passant >> halfmove >> fullmove;

    // 1. Parse piece placement
    int rank = 7;
    int file = 0;

    for (char c : piece_placement)
    {
        if (c == '/')
        {
            rank--;
            file = 0;
        }
        else if (std::isdigit(c))
        {
            file += (c - '0');
        }
        else
        {
            Square sq = static_cast<Square>(rank * 8 + file);
            Color color = std::isupper(c) ? WHITE : BLACK;
            PieceType pt = PAWN;

            switch (std::tolower(c))
            {
                case 'p':
                    pt = PAWN;
                    break;
                case 'n':
                    pt = KNIGHT;
                    break;
                case 'b':
                    pt = BISHOP;
                    break;
                case 'r':
                    pt = ROOK;
                    break;
                case 'q':
                    pt = QUEEN;
                    break;
                case 'k':
                    pt = KING;
                    break;
            }

            Piece piece = make_piece(color, pt);
            Bitboard mask = square_bb(sq);

            pos.pieces[color][pt] |= mask;
            pos.board[sq] = piece;
            add_piece_eval(pos, pt, color, sq);

            file++;
        }
    }

    // Update occupancy bitboards[cite: 6]
    pos.update_occupancy();

    // 2. Parse active color
    pos.sideToMove = (active_color == "w") ? WHITE : BLACK;

    // 3. Parse castling rights
    pos.castlingRights = 0;
    if (castling != "-")
    {
        for (char c : castling)
        {
            if (c == 'K')
                pos.castlingRights |= 1; // White Kingside
            else if (c == 'Q')
                pos.castlingRights |= 2; // White Queenside
            else if (c == 'k')
                pos.castlingRights |= 4; // Black Kingside
            else if (c == 'q')
                pos.castlingRights |= 8; // Black Queenside
        }
    }

    // 4. Parse en passant square
    pos.enPassantSquare = SQ_NONE;
    if (en_passant != "-" && en_passant.length() >= 2)
    {
        int ep_file = en_passant[0] - 'a';
        int ep_rank = en_passant[1] - '1';
        pos.enPassantSquare = static_cast<Square>(ep_rank * 8 + ep_file);
    }

    // 5. Halfmove clock
    pos.halfmoveClock = static_cast<uint16_t>(halfmove);

    // 6. Generate full Zobrist key for the position
    pos.key = Zobrist::generate_key(pos);

    return pos;
}