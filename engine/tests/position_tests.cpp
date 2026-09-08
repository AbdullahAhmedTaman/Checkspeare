/*
 * Catch2 (v3) tests for Position::update_occupancy, Position::make_move,
 * and Position::unmake_move.
 *
 * ---------------------------------------------------------------------
 * ASSUMPTIONS ABOUT THE PUBLIC API (I don't have types.h / move.h)
 * ---------------------------------------------------------------------
 * These are inferred purely from how they're used inside position.cpp.
 * If your real headers differ, the fix is almost always a rename, not a
 * rewrite of the test logic:
 *
 *   - Square, Color, PieceType, Piece are plain (unscoped) enums, using
 *     exactly the names already used in position.cpp: A1..H8, SQ_NONE,
 *     WHITE, BLACK, BOTH, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
 *     PieceType::NONE, WHITE_PAWN..BLACK_KING, PIECE_NONE.
 *   - Bitboard is an unsigned 64-bit integer type.
 *   - set_bit(Bitboard&, Square) / clear_bit(Bitboard&, Square) live in
 *     bitboard.h and flip a single bit.
 *   - Move has a constructor Move(Square from, Square to, int flag) and
 *     from()/to()/flag() accessors. Flags assumed (classic 4-bit move
 *     encoding, contiguous promo range as required by position.cpp's
 *     `flag >= KNIGHT_PROMO && flag <= QUEEN_PROMO_CAP` check):
 *       QUIET, DOUBLE_PAWN_PUSH, KING_CASTLE, QUEEN_CASTLE, CAPTURE,
 *       EP_CAPTURE, KNIGHT_PROMO, BISHOP_PROMO, ROOK_PROMO, QUEEN_PROMO,
 *       KNIGHT_PROMO_CAP, BISHOP_PROMO_CAP, ROOK_PROMO_CAP, QUEEN_PROMO_CAP
 *
 *   NOTE: make_move never branches on a CAPTURE flag -- it detects a
 *   capture purely from board[to] being occupied. So QUIET vs CAPTURE
 *   below is cosmetic; use whichever your "no special flag" constant is
 *   actually called.
 *
 * Build: Catch2 v3, e.g.
 *   target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
 * If you're on Catch2 v2, swap the include for <catch2/catch.hpp> and
 * link against a main you provide (CATCH_CONFIG_MAIN).
 */

#include <catch2/catch_test_macros.hpp>

#include "../include/bitboard.h"
#include "../include/position.h"
#include "../include/types.h"

namespace
{

    constexpr Bitboard sqBit(Square s)
    {
        return 1ULL << static_cast<int>(s);
    }

    /// Pack a Move from its three logical fields, per types.h's documented
    /// encoding (bits 0-5 from, 6-11 to, 12-15 flag).
    inline Move makeMove(Square from, Square to, int flag)
    {
        Move m;
        m.data = static_cast<uint16_t>(static_cast<uint16_t>(from) | (static_cast<uint16_t>(to) << 6) |
                                       (static_cast<uint16_t>(flag) << 12));
        return m;
    }

    /// Zero out a Position completely: empty board, empty bitboards, ply 0.
    void clearPosition(Position& pos)
    {
        for (int c = 0; c < 2; ++c)
            for (int pt = 0; pt < PieceType::NONE; ++pt)
                pos.pieces[c][pt] = BB_EMPTY;

        for (int i = 0; i < 3; ++i)
            pos.occupancy[i] = BB_EMPTY;

        for (int sq = 0; sq < SQ_NONE; ++sq)
            pos.board[sq] = PIECE_NONE;

        pos.sideToMove = WHITE;
        pos.castlingRights = 0;
        pos.enPassantSquare = SQ_NONE;
        pos.halfmoveClock = 0;
        pos.ply = 0;
        pos.key = 0;

        // Reset Track 3 Evaluation arrays
        pos.mg_score[WHITE] = 0;
        pos.mg_score[BLACK] = 0;
        pos.eg_score[WHITE] = 0;
        pos.eg_score[BLACK] = 0;
        pos.game_phase = 0;
    }

    /// Place a piece, keeping pieces[][] and board[] in sync. Caller must call
    /// pos.update_occupancy() afterwards (batched for setups with many pieces).
    void placePiece(Position& pos, Piece piece, Color color, PieceType pt, Square sq)
    {
        // For testing setup, we manually set the bits (since update_occupancy isn't used incrementally here)
        pos.pieces[color][pt] |= sqBit(sq);
        pos.board[sq] = piece;
    }

    /// Deep-compare two positions field by field to ensure perfect rollback.
    bool positionsEqual(const Position& a, const Position& b)
    {
        for (int c = 0; c < 2; ++c)
            for (int pt = 0; pt < PieceType::NONE; ++pt)
                if (a.pieces[c][pt] != b.pieces[c][pt])
                    return false;

        for (int i = 0; i < 3; ++i)
            if (a.occupancy[i] != b.occupancy[i])
                return false;

        for (int sq = 0; sq < SQ_NONE; ++sq)
            if (a.board[sq] != b.board[sq])
                return false;

        // Check evaluation scores (Track 3)
        if (a.mg_score[WHITE] != b.mg_score[WHITE] || a.mg_score[BLACK] != b.mg_score[BLACK])
            return false;
        if (a.eg_score[WHITE] != b.eg_score[WHITE] || a.eg_score[BLACK] != b.eg_score[BLACK])
            return false;
        if (a.game_phase != b.game_phase)
            return false;

        return a.sideToMove == b.sideToMove && a.castlingRights == b.castlingRights &&
               a.enPassantSquare == b.enPassantSquare && a.halfmoveClock == b.halfmoveClock && a.key == b.key &&
               a.ply == b.ply;
    }

    /// Invariant that must hold after every move:
    /// BOTH is the union of WHITE/BLACK, and WHITE/BLACK never overlap.
    void requireOccupancyInvariant(const Position& pos)
    {
        REQUIRE(pos.occupancy[BOTH] == (pos.occupancy[WHITE] | pos.occupancy[BLACK]));
        REQUIRE((pos.occupancy[WHITE] & pos.occupancy[BLACK]) == BB_EMPTY);
    }

    void setupStartingPosition(Position& pos)
    {
        clearPosition(pos);

        const Square whitePawns[] = {A2, B2, C2, D2, E2, F2, G2, H2};
        for (Square s : whitePawns)
            placePiece(pos, WHITE_PAWN, WHITE, PAWN, s);

        placePiece(pos, WHITE_ROOK, WHITE, ROOK, A1);
        placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, B1);
        placePiece(pos, WHITE_BISHOP, WHITE, BISHOP, C1);
        placePiece(pos, WHITE_QUEEN, WHITE, QUEEN, D1);
        placePiece(pos, WHITE_KING, WHITE, KING, E1);
        placePiece(pos, WHITE_BISHOP, WHITE, BISHOP, F1);
        placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, G1);
        placePiece(pos, WHITE_ROOK, WHITE, ROOK, H1);

        const Square blackPawns[] = {A7, B7, C7, D7, E7, F7, G7, H7};
        for (Square s : blackPawns)
            placePiece(pos, BLACK_PAWN, BLACK, PAWN, s);

        placePiece(pos, BLACK_ROOK, BLACK, ROOK, A8);
        placePiece(pos, BLACK_KNIGHT, BLACK, KNIGHT, B8);
        placePiece(pos, BLACK_BISHOP, BLACK, BISHOP, C8);
        placePiece(pos, BLACK_QUEEN, BLACK, QUEEN, D8);
        placePiece(pos, BLACK_KING, BLACK, KING, E8);
        placePiece(pos, BLACK_BISHOP, BLACK, BISHOP, F8);
        placePiece(pos, BLACK_KNIGHT, BLACK, KNIGHT, G8);
        placePiece(pos, BLACK_ROOK, BLACK, ROOK, H8);

        pos.sideToMove = WHITE;
        pos.castlingRights = 0b1111;
        pos.enPassantSquare = SQ_NONE;
        pos.halfmoveClock = 0;
        pos.ply = 0;

        // Manually build initial occupancy since we removed the helper loop
        pos.occupancy[WHITE] = BB_EMPTY;
        pos.occupancy[BLACK] = BB_EMPTY;
        for (int pt = 0; pt < PieceType::NONE; ++pt)
        {
            pos.occupancy[WHITE] |= pos.pieces[WHITE][pt];
            pos.occupancy[BLACK] |= pos.pieces[BLACK][pt];
        }
        pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];
    }

} // namespace

// ---------------------------------------------------------------------
// make_move / unmake_move: quiet moves
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: quiet knight move", "[position][make_move]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, G1);
    pos.occupancy[WHITE] |= sqBit(G1);
    pos.occupancy[BOTH] |= sqBit(G1);
    pos.halfmoveClock = 5;

    Position before = pos;
    Move m = makeMove(G1, F3, QUIET_MOVE);
    pos.make_move(m);

    SECTION("state right after make_move")
    {
        REQUIRE(pos.board[G1] == PIECE_NONE);
        REQUIRE(pos.board[F3] == WHITE_KNIGHT);
        REQUIRE((pos.pieces[WHITE][KNIGHT] & sqBit(G1)) == BB_EMPTY);
        REQUIRE((pos.pieces[WHITE][KNIGHT] & sqBit(F3)) != BB_EMPTY);
        REQUIRE(pos.sideToMove == BLACK);
        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.halfmoveClock == 6); // not a pawn move, not a capture
        REQUIRE(pos.ply == 1);
        requireOccupancyInvariant(pos);
    }

    SECTION("unmake_move restores the exact prior state")
    {
        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

TEST_CASE("make_move: pawn single push resets halfmove clock", "[position][make_move]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_PAWN, WHITE, PAWN, E2);
    pos.occupancy[WHITE] |= sqBit(E2);
    pos.occupancy[BOTH] |= sqBit(E2);
    pos.halfmoveClock = 10;
    Position before = pos;

    Move m = makeMove(E2, E3, QUIET_MOVE);
    pos.make_move(m);

    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.board[E3] == WHITE_PAWN);
    REQUIRE(pos.board[E2] == PIECE_NONE);

    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
}

TEST_CASE("make_move/unmake_move: double pawn push sets en passant target", "[position][make_move][en_passant]")
{
    SECTION("white e2-e4")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_PAWN, WHITE, PAWN, E2);
        pos.occupancy[WHITE] |= sqBit(E2);
        pos.occupancy[BOTH] |= sqBit(E2);
        Position before = pos;

        Move m = makeMove(E2, E4, DOUBLE_PAWN_PUSH);
        pos.make_move(m);

        REQUIRE(pos.enPassantSquare == E3);
        REQUIRE(pos.halfmoveClock == 0);
        REQUIRE(pos.board[E4] == WHITE_PAWN);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }

    SECTION("black e7-e5")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, BLACK_PAWN, BLACK, PAWN, E7);
        pos.occupancy[BLACK] |= sqBit(E7);
        pos.occupancy[BOTH] |= sqBit(E7);
        pos.sideToMove = BLACK;
        Position before = pos;

        Move m = makeMove(E7, E5, DOUBLE_PAWN_PUSH);
        pos.make_move(m);

        REQUIRE(pos.enPassantSquare == E6);
        REQUIRE(pos.halfmoveClock == 0);
        REQUIRE(pos.board[E5] == BLACK_PAWN);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

// ---------------------------------------------------------------------
// make_move / unmake_move: captures
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: standard capture", "[position][make_move][capture]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_BISHOP, WHITE, BISHOP, C4);
    placePiece(pos, BLACK_KNIGHT, BLACK, KNIGHT, F7);
    pos.occupancy[WHITE] |= sqBit(C4);
    pos.occupancy[BLACK] |= sqBit(F7);
    pos.occupancy[BOTH] |= (sqBit(C4) | sqBit(F7));

    pos.halfmoveClock = 8;
    Position before = pos;

    Move m = makeMove(C4, F7, QUIET_MOVE); // capture detected via board[to]
    pos.make_move(m);

    REQUIRE(pos.board[F7] == WHITE_BISHOP);
    REQUIRE(pos.board[C4] == PIECE_NONE);
    REQUIRE((pos.pieces[BLACK][KNIGHT] & sqBit(F7)) == BB_EMPTY);
    REQUIRE((pos.pieces[WHITE][BISHOP] & sqBit(F7)) != BB_EMPTY);
    REQUIRE(pos.halfmoveClock == 0);
    requireOccupancyInvariant(pos);

    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
}

TEST_CASE("make_move/unmake_move: en passant capture", "[position][make_move][en_passant]")
{
    SECTION("white captures black en passant")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_PAWN, WHITE, PAWN, E5);
        placePiece(pos, BLACK_PAWN, BLACK, PAWN, D5);
        pos.occupancy[WHITE] |= sqBit(E5);
        pos.occupancy[BLACK] |= sqBit(D5);
        pos.occupancy[BOTH] |= (sqBit(E5) | sqBit(D5));

        pos.enPassantSquare = D6;
        Position before = pos;

        Move m = makeMove(E5, D6, EP_CAPTURE);
        pos.make_move(m);

        REQUIRE(pos.board[D6] == WHITE_PAWN);
        REQUIRE(pos.board[E5] == PIECE_NONE);
        REQUIRE(pos.board[D5] == PIECE_NONE); // captured pawn correctly removed
        REQUIRE((pos.pieces[BLACK][PAWN] & sqBit(D5)) == BB_EMPTY);
        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.halfmoveClock == 0);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
        REQUIRE(pos.board[D5] == BLACK_PAWN);
    }

    SECTION("black captures white en passant")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, BLACK_PAWN, BLACK, PAWN, C4);
        placePiece(pos, WHITE_PAWN, WHITE, PAWN, B4);
        pos.occupancy[BLACK] |= sqBit(C4);
        pos.occupancy[WHITE] |= sqBit(B4);
        pos.occupancy[BOTH] |= (sqBit(C4) | sqBit(B4));

        pos.sideToMove = BLACK;
        pos.enPassantSquare = B3;
        Position before = pos;

        Move m = makeMove(C4, B3, EP_CAPTURE);
        pos.make_move(m);

        REQUIRE(pos.board[B3] == BLACK_PAWN);
        REQUIRE(pos.board[B4] == PIECE_NONE); // White pawn removed
        REQUIRE((pos.pieces[WHITE][PAWN] & sqBit(B4)) == BB_EMPTY);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

// ---------------------------------------------------------------------
// make_move / unmake_move: castling
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: castling moves king and rook perfectly", "[position][make_move][castling]")
{
    struct CastleCase
    {
        Color color;
        Square kFrom, kTo, rFrom, rTo;
        int flag;
        Piece king, rook;
    };

    const CastleCase cases[] = {{WHITE, E1, G1, H1, F1, KING_CASTLE, WHITE_KING, WHITE_ROOK},
                                {WHITE, E1, C1, A1, D1, QUEEN_CASTLE, WHITE_KING, WHITE_ROOK},
                                {BLACK, E8, G8, H8, F8, KING_CASTLE, BLACK_KING, BLACK_ROOK},
                                {BLACK, E8, C8, A8, D8, QUEEN_CASTLE, BLACK_KING, BLACK_ROOK}};

    for (const auto& c : cases)
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, c.king, c.color, KING, c.kFrom);
        placePiece(pos, c.rook, c.color, ROOK, c.rFrom);
        pos.occupancy[c.color] |= (sqBit(c.kFrom) | sqBit(c.rFrom));
        pos.occupancy[BOTH] |= (sqBit(c.kFrom) | sqBit(c.rFrom));
        pos.sideToMove = c.color;

        Position before = pos;
        Move m = makeMove(c.kFrom, c.kTo, c.flag);
        pos.make_move(m);

        REQUIRE(pos.board[c.kTo] == c.king);
        REQUIRE(pos.board[c.rTo] == c.rook);
        REQUIRE(pos.board[c.kFrom] == PIECE_NONE);
        REQUIRE(pos.board[c.rFrom] == PIECE_NONE);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

// ---------------------------------------------------------------------
// make_move / unmake_move: promotion
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: promotion and promotion-captures", "[position][make_move][promotion]")
{
    struct PromoCase
    {
        int flag;
        PieceType pt;
        Piece whitePiece;
    };

    const PromoCase cases[] = {{KNIGHT_PROMO, KNIGHT, WHITE_KNIGHT},     {BISHOP_PROMO, BISHOP, WHITE_BISHOP},
                               {ROOK_PROMO, ROOK, WHITE_ROOK},           {QUEEN_PROMO, QUEEN, WHITE_QUEEN},
                               {KNIGHT_PROMO_CAP, KNIGHT, WHITE_KNIGHT}, {BISHOP_PROMO_CAP, BISHOP, WHITE_BISHOP},
                               {ROOK_PROMO_CAP, ROOK, WHITE_ROOK},       {QUEEN_PROMO_CAP, QUEEN, WHITE_QUEEN}};

    for (const auto& c : cases)
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_PAWN, WHITE, PAWN, B7);
        pos.occupancy[WHITE] |= sqBit(B7);
        pos.occupancy[BOTH] |= sqBit(B7);

        bool isCapture = (c.flag >= KNIGHT_PROMO_CAP);
        if (isCapture)
        {
            placePiece(pos, BLACK_ROOK, BLACK, ROOK, A8);
            pos.occupancy[BLACK] |= sqBit(A8);
            pos.occupancy[BOTH] |= sqBit(A8);
        }

        Position before = pos;
        Move m = makeMove(B7, isCapture ? A8 : B8, c.flag);
        pos.make_move(m);

        Square toSq = isCapture ? A8 : B8;
        REQUIRE(pos.board[toSq] == c.whitePiece);
        REQUIRE((pos.pieces[WHITE][c.pt] & sqBit(toSq)) != BB_EMPTY);
        REQUIRE((pos.pieces[WHITE][PAWN] & sqBit(toSq)) == BB_EMPTY);
        REQUIRE(pos.halfmoveClock == 0);

        if (isCapture)
        {
            REQUIRE((pos.pieces[BLACK][ROOK] & sqBit(toSq)) == BB_EMPTY);
            REQUIRE(pos.history[0].capturedPiece == BLACK_ROOK);
        }

        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
        REQUIRE(pos.board[B7] == WHITE_PAWN);
    }
}

// ---------------------------------------------------------------------
// Evaluation History stack contents
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: Track 3 Evaluation State is perfectly maintained", "[position][make_move][eval]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, B1);
    pos.occupancy[WHITE] |= sqBit(B1);
    pos.occupancy[BOTH] |= sqBit(B1);

    // Inject mock evaluation scores directly into the board
    pos.mg_score[WHITE] = 1200;
    pos.mg_score[BLACK] = 1150;
    pos.eg_score[WHITE] = 800;
    pos.eg_score[BLACK] = 750;
    pos.game_phase = 24;

    Position before = pos;
    Move m = makeMove(B1, C3, QUIET_MOVE);

    // 1. Ensure the stack pushed the exact values BEFORE the math executed
    pos.make_move(m);

    const UndoState& saved = pos.history[0];
    REQUIRE(saved.mg_score[WHITE] == 1200);
    REQUIRE(saved.mg_score[BLACK] == 1150);
    REQUIRE(saved.eg_score[WHITE] == 800);
    REQUIRE(saved.eg_score[BLACK] == 750);
    REQUIRE(saved.game_phase == 24);

    // 2. Ensure Track 3's evaluation math was executed (the score should have changed)
    // Even without knowing the exact PST math from evaluate.h, we know the score will mutate
    bool scoreMutated = (pos.mg_score[WHITE] != 1200) || (pos.eg_score[WHITE] != 800);
    REQUIRE(scoreMutated == true);

    // 3. Rollback the move and guarantee the values were snapped back in $O(1)$
    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
}

// ---------------------------------------------------------------------
// Known limitations
// ---------------------------------------------------------------------

TEST_CASE("KNOWN LIMITATION: castlingRights is not updated by make_move", "[position][make_move][known_limitation]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KING, WHITE, KING, E1);
    placePiece(pos, WHITE_ROOK, WHITE, ROOK, H1);
    pos.castlingRights = 0b1111;

    Move kingMove = makeMove(E1, E2, QUIET_MOVE);
    pos.make_move(kingMove);

    REQUIRE(pos.castlingRights == 0b1111); // unchanged -- not yet implemented
}

TEST_CASE("KNOWN LIMITATION: key is not incrementally updated by make_move", "[position][make_move][known_limitation]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_PAWN, WHITE, PAWN, E2);
    pos.key = 0x123456789ABCDEF0ULL;

    Move m = makeMove(E2, E4, DOUBLE_PAWN_PUSH);
    pos.make_move(m);

    REQUIRE(pos.key == 0x123456789ABCDEF0ULL); // unchanged -- no Zobrist XORs yet
}
