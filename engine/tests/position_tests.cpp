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
    }

    /// Place a piece, keeping pieces[][] and board[] in sync. Caller must call
    /// pos.update_occupancy() afterwards (batched for setups with many pieces).
    void placePiece(Position& pos, Piece piece, Color color, PieceType pt, Square sq)
    {
        set_bit(pos.pieces[color][pt], sq);
        pos.board[sq] = piece;
    }

    /// Deep-compare two positions field by field.
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

        return a.sideToMove == b.sideToMove && a.castlingRights == b.castlingRights && a.enPassantSquare == b.enPassantSquare &&
               a.halfmoveClock == b.halfmoveClock && a.key == b.key && a.ply == b.ply;
    }

    /// Invariant that must hold after every update_occupancy() call:
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

        pos.update_occupancy();
    }

} // namespace

// ---------------------------------------------------------------------
// update_occupancy
// ---------------------------------------------------------------------

TEST_CASE("update_occupancy: empty position yields empty occupancy", "[position][occupancy]")
{
    Position pos;
    clearPosition(pos);

    pos.update_occupancy();

    REQUIRE(pos.occupancy[WHITE] == BB_EMPTY);
    REQUIRE(pos.occupancy[BLACK] == BB_EMPTY);
    REQUIRE(pos.occupancy[BOTH] == BB_EMPTY);
}

TEST_CASE("update_occupancy: single piece per color", "[position][occupancy]")
{
    Position pos;
    clearPosition(pos);

    placePiece(pos, WHITE_KING, WHITE, KING, E1);
    placePiece(pos, BLACK_KING, BLACK, KING, E8);

    pos.update_occupancy();

    REQUIRE(pos.occupancy[WHITE] == sqBit(E1));
    REQUIRE(pos.occupancy[BLACK] == sqBit(E8));
    requireOccupancyInvariant(pos);
}

TEST_CASE("update_occupancy: multiple piece types combine correctly", "[position][occupancy]")
{
    Position pos;
    clearPosition(pos);

    placePiece(pos, WHITE_PAWN, WHITE, PAWN, E4);
    placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, F3);
    placePiece(pos, WHITE_KING, WHITE, KING, G1);
    placePiece(pos, BLACK_PAWN, BLACK, PAWN, D5);
    placePiece(pos, BLACK_BISHOP, BLACK, BISHOP, C5);
    placePiece(pos, BLACK_KING, BLACK, KING, G8);

    pos.update_occupancy();

    Bitboard expectedWhite = sqBit(E4) | sqBit(F3) | sqBit(G1);
    Bitboard expectedBlack = sqBit(D5) | sqBit(C5) | sqBit(G8);

    REQUIRE(pos.occupancy[WHITE] == expectedWhite);
    REQUIRE(pos.occupancy[BLACK] == expectedBlack);
    REQUIRE(pos.occupancy[BOTH] == (expectedWhite | expectedBlack));
    requireOccupancyInvariant(pos);
}

TEST_CASE("update_occupancy: full starting position", "[position][occupancy]")
{
    Position pos;
    setupStartingPosition(pos);

    Bitboard expectedWhite = BB_EMPTY;
    for (Square s : {A1, B1, C1, D1, E1, F1, G1, H1, A2, B2, C2, D2, E2, F2, G2, H2})
        expectedWhite |= sqBit(s);

    Bitboard expectedBlack = BB_EMPTY;
    for (Square s : {A7, B7, C7, D7, E7, F7, G7, H7, A8, B8, C8, D8, E8, F8, G8, H8})
        expectedBlack |= sqBit(s);

    REQUIRE(pos.occupancy[WHITE] == expectedWhite);
    REQUIRE(pos.occupancy[BLACK] == expectedBlack);
    REQUIRE(pos.occupancy[BOTH] == (expectedWhite | expectedBlack));
    requireOccupancyInvariant(pos);
}

// ---------------------------------------------------------------------
// make_move / unmake_move: quiet moves
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: quiet knight move", "[position][make_move]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, G1);
    pos.update_occupancy();
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
    pos.update_occupancy();
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
        pos.update_occupancy();
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
        pos.update_occupancy();
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
    pos.update_occupancy();
    pos.halfmoveClock = 8;
    Position before = pos;

    Move m = makeMove(C4, F7, QUIET_MOVE); // capture detected via board[to]; flag is cosmetic here
    pos.make_move(m);

    REQUIRE(pos.board[F7] == WHITE_BISHOP);
    REQUIRE(pos.board[C4] == PIECE_NONE);
    REQUIRE((pos.pieces[BLACK][KNIGHT] & sqBit(F7)) == BB_EMPTY);
    REQUIRE((pos.pieces[WHITE][BISHOP] & sqBit(F7)) != BB_EMPTY);
    REQUIRE(pos.halfmoveClock == 0);
    requireOccupancyInvariant(pos);

    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
    REQUIRE(pos.board[F7] == BLACK_KNIGHT);
    REQUIRE(pos.board[C4] == WHITE_BISHOP);
}

TEST_CASE("make_move: history stack records the captured piece", "[position][make_move][history]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_QUEEN, WHITE, QUEEN, D1);
    placePiece(pos, BLACK_BISHOP, BLACK, BISHOP, D8);
    pos.update_occupancy();

    Move m = makeMove(D1, D8, QUIET_MOVE);
    pos.make_move(m);

    REQUIRE(pos.history[0].capturedPiece == BLACK_BISHOP);
}

TEST_CASE("make_move/unmake_move: en passant capture", "[position][make_move][en_passant]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_PAWN, WHITE, PAWN, E5);
    placePiece(pos, BLACK_PAWN, BLACK, PAWN, D5);
    pos.update_occupancy();
    pos.enPassantSquare = D6; // as if black just played d7-d5
    Position before = pos;

    Move m = makeMove(E5, D6, EP_CAPTURE);
    pos.make_move(m);

    REQUIRE(pos.board[D6] == WHITE_PAWN);
    REQUIRE(pos.board[E5] == PIECE_NONE);
    REQUIRE(pos.board[D5] == PIECE_NONE); // the captured pawn, not on the "to" square
    REQUIRE((pos.pieces[BLACK][PAWN] & sqBit(D5)) == BB_EMPTY);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    requireOccupancyInvariant(pos);

    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
    REQUIRE(pos.board[D5] == BLACK_PAWN);
    REQUIRE(pos.board[E5] == WHITE_PAWN);
    REQUIRE(pos.board[D6] == PIECE_NONE);
}

// ---------------------------------------------------------------------
// make_move / unmake_move: castling
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: king-side castling", "[position][make_move][castling]")
{
    SECTION("white")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_KING, WHITE, KING, E1);
        placePiece(pos, WHITE_ROOK, WHITE, ROOK, H1);
        pos.update_occupancy();
        Position before = pos;

        Move m = makeMove(E1, G1, KING_CASTLE);
        pos.make_move(m);

        REQUIRE(pos.board[G1] == WHITE_KING);
        REQUIRE(pos.board[F1] == WHITE_ROOK);
        REQUIRE(pos.board[E1] == PIECE_NONE);
        REQUIRE(pos.board[H1] == PIECE_NONE);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }

    SECTION("black")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, BLACK_KING, BLACK, KING, E8);
        placePiece(pos, BLACK_ROOK, BLACK, ROOK, H8);
        pos.update_occupancy();
        pos.sideToMove = BLACK;
        Position before = pos;

        Move m = makeMove(E8, G8, KING_CASTLE);
        pos.make_move(m);

        REQUIRE(pos.board[G8] == BLACK_KING);
        REQUIRE(pos.board[F8] == BLACK_ROOK);
        REQUIRE(pos.board[E8] == PIECE_NONE);
        REQUIRE(pos.board[H8] == PIECE_NONE);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

TEST_CASE("make_move/unmake_move: queen-side castling", "[position][make_move][castling]")
{
    SECTION("white")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_KING, WHITE, KING, E1);
        placePiece(pos, WHITE_ROOK, WHITE, ROOK, A1);
        pos.update_occupancy();
        Position before = pos;

        Move m = makeMove(E1, C1, QUEEN_CASTLE);
        pos.make_move(m);

        REQUIRE(pos.board[C1] == WHITE_KING);
        REQUIRE(pos.board[D1] == WHITE_ROOK);
        REQUIRE(pos.board[E1] == PIECE_NONE);
        REQUIRE(pos.board[A1] == PIECE_NONE);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }

    SECTION("black")
    {
        Position pos;
        clearPosition(pos);
        placePiece(pos, BLACK_KING, BLACK, KING, E8);
        placePiece(pos, BLACK_ROOK, BLACK, ROOK, A8);
        pos.update_occupancy();
        pos.sideToMove = BLACK;
        Position before = pos;

        Move m = makeMove(E8, C8, QUEEN_CASTLE);
        pos.make_move(m);

        REQUIRE(pos.board[C8] == BLACK_KING);
        REQUIRE(pos.board[D8] == BLACK_ROOK);
        REQUIRE(pos.board[E8] == PIECE_NONE);
        REQUIRE(pos.board[A8] == PIECE_NONE);
        requireOccupancyInvariant(pos);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
    }
}

// ---------------------------------------------------------------------
// make_move / unmake_move: promotion
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: promotion to every piece type", "[position][make_move][promotion]")
{
    struct Case
    {
        int flag;
        PieceType pt;
        Piece whitePiece;
    };

    const Case cases[] = {
        {KNIGHT_PROMO, KNIGHT, WHITE_KNIGHT},
        {BISHOP_PROMO, BISHOP, WHITE_BISHOP},
        {ROOK_PROMO, ROOK, WHITE_ROOK},
        {QUEEN_PROMO, QUEEN, WHITE_QUEEN},
    };

    for (const auto& c : cases)
    {
        INFO("promotion flag = " << c.flag);

        Position pos;
        clearPosition(pos);
        placePiece(pos, WHITE_PAWN, WHITE, PAWN, G7);
        pos.update_occupancy();
        Position before = pos;

        Move m = makeMove(G7, G8, c.flag);
        pos.make_move(m);

        REQUIRE(pos.board[G8] == c.whitePiece);
        REQUIRE((pos.pieces[WHITE][c.pt] & sqBit(G8)) != BB_EMPTY);
        REQUIRE((pos.pieces[WHITE][PAWN] & sqBit(G8)) == BB_EMPTY);
        REQUIRE(pos.halfmoveClock == 0);

        pos.unmake_move(m);
        REQUIRE(positionsEqual(pos, before));
        REQUIRE(pos.board[G7] == WHITE_PAWN);
        REQUIRE(pos.board[G8] == PIECE_NONE);
    }
}

TEST_CASE("make_move/unmake_move: promotion capture", "[position][make_move][promotion][capture]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_PAWN, WHITE, PAWN, B7);
    placePiece(pos, BLACK_ROOK, BLACK, ROOK, A8);
    pos.update_occupancy();
    Position before = pos;

    Move m = makeMove(B7, A8, QUEEN_PROMO_CAP);
    pos.make_move(m);

    REQUIRE(pos.board[A8] == WHITE_QUEEN);
    REQUIRE((pos.pieces[BLACK][ROOK] & sqBit(A8)) == BB_EMPTY);
    REQUIRE((pos.pieces[WHITE][QUEEN] & sqBit(A8)) != BB_EMPTY);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.history[0].capturedPiece == BLACK_ROOK);
    requireOccupancyInvariant(pos);

    pos.unmake_move(m);
    REQUIRE(positionsEqual(pos, before));
    REQUIRE(pos.board[A8] == BLACK_ROOK);
    REQUIRE(pos.board[B7] == WHITE_PAWN);
}

// ---------------------------------------------------------------------
// make_move / unmake_move: multi-move stack integrity
// ---------------------------------------------------------------------

TEST_CASE("make_move/unmake_move: multi-move sequence fully reverses", "[position][make_move][stack]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KING, WHITE, KING, E1);
    placePiece(pos, BLACK_KING, BLACK, KING, E8);
    placePiece(pos, WHITE_PAWN, WHITE, PAWN, E2);
    placePiece(pos, BLACK_PAWN, BLACK, PAWN, D7);
    pos.update_occupancy();
    Position original = pos;

    Move m1 = makeMove(E2, E4, DOUBLE_PAWN_PUSH);
    Move m2 = makeMove(D7, D5, DOUBLE_PAWN_PUSH);
    Move m3 = makeMove(E1, F1, QUIET_MOVE);
    Move m4 = makeMove(E8, F8, QUIET_MOVE);

    pos.make_move(m1);
    REQUIRE(pos.ply == 1);
    pos.make_move(m2);
    REQUIRE(pos.ply == 2);
    pos.make_move(m3);
    REQUIRE(pos.ply == 3);
    pos.make_move(m4);
    REQUIRE(pos.ply == 4);
    requireOccupancyInvariant(pos);

    pos.unmake_move(m4);
    pos.unmake_move(m3);
    pos.unmake_move(m2);
    pos.unmake_move(m1);

    REQUIRE(pos.ply == 0);
    REQUIRE(positionsEqual(pos, original));
}

TEST_CASE("make_move/unmake_move: short opening sequence from the starting position", "[position][make_move][integration]")
{
    Position pos;
    setupStartingPosition(pos);
    Position original = pos;

    Move moves[] = {
        makeMove(E2, E4, DOUBLE_PAWN_PUSH), makeMove(E7, E5, DOUBLE_PAWN_PUSH), makeMove(G1, F3, QUIET_MOVE),
        makeMove(B8, C6, QUIET_MOVE),       makeMove(F1, B5, QUIET_MOVE),
    };

    for (const Move& m : moves)
        pos.make_move(m);

    REQUIRE(pos.ply == 5);
    REQUIRE(pos.board[E4] == WHITE_PAWN);
    REQUIRE(pos.board[E5] == BLACK_PAWN);
    REQUIRE(pos.board[F3] == WHITE_KNIGHT);
    REQUIRE(pos.board[C6] == BLACK_KNIGHT);
    REQUIRE(pos.board[B5] == WHITE_BISHOP);
    REQUIRE(pos.sideToMove == BLACK);
    requireOccupancyInvariant(pos);

    for (int i = 4; i >= 0; --i)
        pos.unmake_move(moves[i]);

    REQUIRE(pos.ply == 0);
    REQUIRE(positionsEqual(pos, original));
}

// ---------------------------------------------------------------------
// History stack contents
// ---------------------------------------------------------------------

TEST_CASE("make_move: history stack stores the exact pre-move state", "[position][make_move][history]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KNIGHT, WHITE, KNIGHT, B1);
    pos.update_occupancy();
    pos.castlingRights = 0b1111;
    pos.enPassantSquare = SQ_NONE;
    pos.halfmoveClock = 3;
    pos.key = 0xDEADBEEFULL;

    Move m = makeMove(B1, D2, QUIET_MOVE);
    pos.make_move(m);

    const UndoState& saved = pos.history[0];
    REQUIRE(saved.castlingRights == 0b1111);
    REQUIRE(saved.enPassantSquare == SQ_NONE);
    REQUIRE(saved.halfmoveClock == 3);
    REQUIRE(saved.key == 0xDEADBEEFULL);
    REQUIRE(saved.capturedPiece == PIECE_NONE);
}

// ---------------------------------------------------------------------
// Known limitations (documents current behavior, not a spec)
//
// These pass today because make_move genuinely leaves castlingRights/key
// untouched -- it only ever saves/restores them via the undo stack. If you
// add real castling-rights maintenance or incremental Zobrist hashing
// later, these tests SHOULD start failing; that's your cue to rewrite them
// against the new rules and delete this comment block.
// ---------------------------------------------------------------------

TEST_CASE("KNOWN LIMITATION: castlingRights is not updated by make_move", "[position][make_move][known_limitation]")
{
    Position pos;
    clearPosition(pos);
    placePiece(pos, WHITE_KING, WHITE, KING, E1);
    placePiece(pos, WHITE_ROOK, WHITE, ROOK, H1);
    pos.update_occupancy();
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
    pos.update_occupancy();
    pos.key = 0x123456789ABCDEF0ULL;

    Move m = makeMove(E2, E4, DOUBLE_PAWN_PUSH);
    pos.make_move(m);

    REQUIRE(pos.key == 0x123456789ABCDEF0ULL); // unchanged -- no Zobrist XORs yet
}

// ---------------------------------------------------------------------
// from_fen
// ---------------------------------------------------------------------

TEST_CASE("from_fen: parses the starting position correctly", "[position][fen]")
{
    // The standard chess initial position
    Position pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0b1111); // K, Q, k, q all available
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);

    // Verify piece placement anchors
    REQUIRE(pos.board[A1] == WHITE_ROOK);
    REQUIRE(pos.board[E1] == WHITE_KING);
    REQUIRE(pos.board[A8] == BLACK_ROOK);
    REQUIRE(pos.board[E8] == BLACK_KING);
    REQUIRE(pos.board[E2] == WHITE_PAWN);
    REQUIRE(pos.board[E7] == BLACK_PAWN);

    // Verify empty squares
    REQUIRE(pos.board[E4] == PIECE_NONE);
    REQUIRE(pos.board[D5] == PIECE_NONE);
}

TEST_CASE("from_fen: parses complex middlegame states", "[position][fen]")
{
    // Position with Black to move, an en passant target, partial castling rights, and an advanced clock
    Position pos = Position::from_fen("r1bqkbnr/ppp1p1pp/2n5/3P1p2/8/5N2/PPPP1PPP/RNBQKB1R b KQq f3 14 5");

    REQUIRE(pos.sideToMove == BLACK);

    // Castling rights: White Kingside (1), White Queenside (2), Black Queenside (8) -> 1 | 2 | 8 = 11
    REQUIRE(pos.castlingRights == 11);

    // En passant target is f3
    REQUIRE(pos.enPassantSquare == F3);

    // Halfmove clock advanced
    REQUIRE(pos.halfmoveClock == 14);

    // Verify specific piece placements from the FEN
    REQUIRE(pos.board[C6] == BLACK_KNIGHT);
    REQUIRE(pos.board[D5] == WHITE_PAWN);
    REQUIRE(pos.board[F5] == BLACK_PAWN);
    REQUIRE(pos.board[F3] == WHITE_KNIGHT);
}

TEST_CASE("from_fen: handles endgame states and stripped rights", "[position][fen]")
{
    // Endgame state with no castling rights, no en passant, and high halfmove clock
    Position pos = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w - - 49 100");

    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 49);

    // Only kings remain on the board
    REQUIRE(pos.board[E1] == WHITE_KING);
    REQUIRE(pos.board[E8] == BLACK_KING);
    REQUIRE(pos.board[A1] == PIECE_NONE);
    REQUIRE(pos.board[A8] == PIECE_NONE);
}

TEST_CASE("from_fen: correctly synchronizes bitboards and piece counts", "[position][fen]")
{
    // Validate that the piece bitboards and occupancy match the board array
    Position pos = Position::from_fen("8/5k2/8/8/8/8/2K5/8 w - - 0 1");

    // Check individual piece bitboards
    REQUIRE((pos.pieces[WHITE][KING] & sqBit(C2)) != BB_EMPTY);
    REQUIRE((pos.pieces[BLACK][KING] & sqBit(F7)) != BB_EMPTY);

    // Check occupancy bitboards
    REQUIRE(pos.occupancy[WHITE] == sqBit(C2));
    REQUIRE(pos.occupancy[BLACK] == sqBit(F7));
    REQUIRE(pos.occupancy[BOTH] == (sqBit(C2) | sqBit(F7)));
}