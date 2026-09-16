#include <catch2/catch_test_macros.hpp>

#include "attacks.h"
#include "bitboard.h"
#include "position.h"
#include "types.h"
#include "zobrist.h"

#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// One-time Zobrist init before any TEST_CASE runs.
// ----------------------------------------------------------------------------
namespace
{
    struct ZobristBootstrap
    {
        ZobristBootstrap()
        {
            Zobrist::init();
            init_attacks();
        }
    } g_zobristBootstrap;
} // namespace

// ============================================================================
// Helpers
// ============================================================================
namespace TestUtil
{
    inline Piece char_to_piece(char c)
    {
        switch (c)
        {
            case 'P':
                return WHITE_PAWN;
            case 'N':
                return WHITE_KNIGHT;
            case 'B':
                return WHITE_BISHOP;
            case 'R':
                return WHITE_ROOK;
            case 'Q':
                return WHITE_QUEEN;
            case 'K':
                return WHITE_KING;
            case 'p':
                return BLACK_PAWN;
            case 'n':
                return BLACK_KNIGHT;
            case 'b':
                return BLACK_BISHOP;
            case 'r':
                return BLACK_ROOK;
            case 'q':
                return BLACK_QUEEN;
            case 'k':
                return BLACK_KING;
            default:
                return PIECE_NONE;
        }
    }

    inline Square algebraic_to_square(const std::string& s)
    {
        if (s.size() < 2)
            return SQ_NONE;
        int file = s[0] - 'a';
        int rank = s[1] - '1';
        return static_cast<Square>(rank * 8 + file);
    }

    // Places/replaces a piece, keeping board/bitboards/occupancy in sync.
    // Does NOT touch pos.key.
    inline void set_piece(Position& pos, Piece p, Square sq)
    {
        Piece existing = pos.board[sq];
        if (existing != PIECE_NONE)
        {
            Color ec = color_of(existing);
            pos.pieces[ec][type_of(existing)] &= ~square_bb(sq);
            pos.occupancy[ec] &= ~square_bb(sq);
        }
        pos.board[sq] = p;
        if (p != PIECE_NONE)
        {
            Color c = color_of(p);
            pos.pieces[c][type_of(p)] |= square_bb(sq);
            pos.occupancy[c] |= square_bb(sq);
        }
        pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];
    }

    // Compares every logical field. Excludes history[] (unmake only rewinds
    // ply; stale entries above it are intentionally left in place).
    inline bool positions_equal(const Position& a, const Position& b)
    {
        for (int c = 0; c < 2; ++c)
            for (int pt = 0; pt < PieceType::NONE; ++pt)
                if (a.pieces[c][pt] != b.pieces[c][pt])
                    return false;

        for (int i = 0; i < 3; ++i)
            if (a.occupancy[i] != b.occupancy[i])
                return false;

        for (auto sq : all_squares())
            if (a.board[sq] != b.board[sq])
                return false;

        return a.sideToMove == b.sideToMove && a.castlingRights == b.castlingRights && a.enPassantSquare == b.enPassantSquare &&
               a.halfmoveClock == b.halfmoveClock && a.key == b.key && a.mg_score[WHITE] == b.mg_score[WHITE] &&
               a.mg_score[BLACK] == b.mg_score[BLACK] && a.eg_score[WHITE] == b.eg_score[WHITE] &&
               a.eg_score[BLACK] == b.eg_score[BLACK] && a.game_phase == b.game_phase && a.ply == b.ply;
    }

    const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    inline std::vector<Move> opening_sequence()
    {
        return {
            Move(E2, E4, DOUBLE_PAWN_PUSH), // 1. e4
            Move(E7, E5, DOUBLE_PAWN_PUSH), // 1... e5
            Move(G1, F3, QUIET_MOVE),       // 2. Nf3
            Move(B8, C6, QUIET_MOVE),       // 2... Nc6
            Move(F1, B5, QUIET_MOVE),       // 3. Bb5
            Move(A7, A6, QUIET_MOVE),       // 3... a6
            Move(B5, C6, CAPTURE),          // 4. Bxc6
            Move(D7, C6, CAPTURE),          // 4... dxc6
        };
    }
} // namespace TestUtil

using namespace TestUtil;

// ============================================================================
// A. Move encoding (types.h)
// ============================================================================

TEST_CASE("Move exhaustively round-trips every square/flag combination", "[types][move]")
{
    for (int from = 0; from <= 63; ++from)
        for (int to = 0; to <= 63; ++to)
            for (int flagVal = 0; flagVal <= 15; ++flagVal)
            {
                Move m(static_cast<Square>(from), static_cast<Square>(to), static_cast<MoveFlag>(flagVal));
                REQUIRE(m.from() == static_cast<Square>(from));
                REQUIRE(m.to() == static_cast<Square>(to));
                REQUIRE(m.flag() == flagVal);
            }
}

TEST_CASE("Move equality / inequality", "[types][move]")
{
    Move a(E2, E4, DOUBLE_PAWN_PUSH);
    Move b(E2, E4, DOUBLE_PAWN_PUSH);
    Move c(E2, E4, QUIET_MOVE);
    Move d(D2, D4, DOUBLE_PAWN_PUSH);

    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
    REQUIRE(a != c);
    REQUIRE(a != d);
}

TEST_CASE("Move::none and Move::null sentinels", "[types][move]")
{
    Move n = Move::none();
    Move z = Move::null();

    REQUIRE_FALSE(n.is_ok());
    REQUIRE_FALSE(n.is_null());
    REQUIRE_FALSE(n.is_promotion());
    REQUIRE_FALSE(n.is_capture());

    REQUIRE(z.is_ok());
    REQUIRE(z.is_null());
    REQUIRE_FALSE(z.is_promotion());
    REQUIRE_FALSE(z.is_capture());
    REQUIRE_FALSE(z.is_castle());
    REQUIRE_FALSE(z.is_ep());
    REQUIRE_FALSE(z.is_double_push());
}

TEST_CASE("Move::is_* accessors match flags", "[types][move]")
{
    SECTION("Quiet move")
    {
        Move m(E2, E4, QUIET_MOVE);
        REQUIRE_FALSE(m.is_capture());
        REQUIRE_FALSE(m.is_promotion());
        REQUIRE_FALSE(m.is_castle());
        REQUIRE_FALSE(m.is_ep());
        REQUIRE_FALSE(m.is_double_push());
    }
    SECTION("Capture")
    {
        Move m(E4, D5, CAPTURE);
        REQUIRE(m.is_capture());
        REQUIRE_FALSE(m.is_promotion());
    }
    SECTION("Double push")
    {
        Move m(E2, E4, DOUBLE_PAWN_PUSH);
        REQUIRE(m.is_double_push());
    }
    SECTION("EP capture")
    {
        Move m(E5, D6, EP_CAPTURE);
        REQUIRE(m.is_ep());
        REQUIRE(m.is_capture());
    }
    SECTION("Kingside castle")
    {
        Move m(E1, G1, KING_CASTLE);
        REQUIRE(m.is_castle());
        REQUIRE(m.is_kingside_castle());
        REQUIRE_FALSE(m.is_queenside_castle());
    }
    SECTION("Queenside castle")
    {
        Move m(E1, C1, QUEEN_CASTLE);
        REQUIRE(m.is_castle());
        REQUIRE_FALSE(m.is_kingside_castle());
        REQUIRE(m.is_queenside_castle());
    }
    SECTION("Quiet promotion")
    {
        Move m(A7, A8, KNIGHT_PROMO);
        REQUIRE(m.is_promotion());
        REQUIRE_FALSE(m.is_capture());
        REQUIRE(promotion_type(m) == KNIGHT);
    }
    SECTION("Capture promotion")
    {
        Move m(A7, B8, QUEEN_PROMO_CAP);
        REQUIRE(m.is_promotion());
        REQUIRE(m.is_capture());
        REQUIRE(promotion_type(m) == QUEEN);
    }
}

TEST_CASE("promotion_type maps each promotion flag", "[types][move]")
{
    REQUIRE(promotion_type(Move(A7, A8, KNIGHT_PROMO)) == KNIGHT);
    REQUIRE(promotion_type(Move(A7, A8, BISHOP_PROMO)) == BISHOP);
    REQUIRE(promotion_type(Move(A7, A8, ROOK_PROMO)) == ROOK);
    REQUIRE(promotion_type(Move(A7, A8, QUEEN_PROMO)) == QUEEN);
    REQUIRE(promotion_type(Move(A7, B8, KNIGHT_PROMO_CAP)) == KNIGHT);
    REQUIRE(promotion_type(Move(A7, B8, BISHOP_PROMO_CAP)) == BISHOP);
    REQUIRE(promotion_type(Move(A7, B8, ROOK_PROMO_CAP)) == ROOK);
    REQUIRE(promotion_type(Move(A7, B8, QUEEN_PROMO_CAP)) == QUEEN);
}

// ============================================================================
// B. Position construction / clear
// ============================================================================

TEST_CASE("Position default-constructs to a well-defined empty state", "[position]")
{
    Position pos;

    for (auto sq : all_squares())
        REQUIRE(pos.board[sq] == PIECE_NONE);

    for (int c = 0; c < 2; ++c)
        for (int pt = 0; pt < PieceType::NONE; ++pt)
            REQUIRE(pos.pieces[c][pt] == 0);

    REQUIRE(pos.occupancy[WHITE] == 0);
    REQUIRE(pos.occupancy[BLACK] == 0);
    REQUIRE(pos.occupancy[BOTH] == 0);
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.key == 0);
    REQUIRE(pos.ply == 0);
}

TEST_CASE("Position::clear resets every logical field", "[position]")
{
    Position pos = Position::from_fen(START_FEN);
    pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));

    pos.clear();

    for (auto sq : all_squares())
        REQUIRE(pos.board[sq] == PIECE_NONE);
    REQUIRE(pos.occupancy[BOTH] == 0);
    REQUIRE(pos.sideToMove == WHITE);
    REQUIRE(pos.castlingRights == 0);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.key == 0);
    REQUIRE(pos.ply == 0);
}

TEST_CASE("update_occupancy recomputes from piece bitboards", "[position]")
{
    Position pos = Position::from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    // Wipe occupancy and recompute.
    pos.occupancy[WHITE] = 0;
    pos.occupancy[BLACK] = 0;
    pos.occupancy[BOTH] = 0;
    pos.update_occupancy();

    Bitboard expectedWhite = square_bb(A1) | square_bb(E1) | square_bb(H1);
    Bitboard expectedBlack = square_bb(A8) | square_bb(E8) | square_bb(H8);
    REQUIRE(pos.occupancy[WHITE] == expectedWhite);
    REQUIRE(pos.occupancy[BLACK] == expectedBlack);
    REQUIRE(pos.occupancy[BOTH] == (expectedWhite | expectedBlack));
}

// ============================================================================
// C. Zobrist
// ============================================================================

TEST_CASE("Zobrist::init populates every table", "[zobrist]")
{
    bool anyPiece = false;
    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            if (piece_keys[p][s] != 0)
                anyPiece = true;
    REQUIRE(anyPiece);

    REQUIRE(side_key != 0);

    bool anyCastle = false;
    for (int i = 0; i < 16; ++i)
        if (castle_keys[i] != 0)
            anyCastle = true;
    REQUIRE(anyCastle);

    bool anyEp = false;
    for (int i = 0; i < 8; ++i)
        if (en_passant_keys[i] != 0)
            anyEp = true;
    REQUIRE(anyEp);
}

TEST_CASE("Zobrist::init is deterministic", "[zobrist]")
{
    Zobrist::init();

    Key pieceSnap[12][64];
    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            pieceSnap[p][s] = piece_keys[p][s];
    Key sideSnap = side_key;
    Key castleSnap[16];
    for (int i = 0; i < 16; ++i)
        castleSnap[i] = castle_keys[i];
    Key epSnap[8];
    for (int i = 0; i < 8; ++i)
        epSnap[i] = en_passant_keys[i];

    Zobrist::init();

    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            REQUIRE(piece_keys[p][s] == pieceSnap[p][s]);
    REQUIRE(side_key == sideSnap);
    for (int i = 0; i < 16; ++i)
        REQUIRE(castle_keys[i] == castleSnap[i]);
    for (int i = 0; i < 8; ++i)
        REQUIRE(en_passant_keys[i] == epSnap[i]);
}

TEST_CASE("Zobrist key space has no collisions for the fixed seed", "[zobrist]")
{
    std::set<Key> seen;
    size_t total = 0;
    auto add = [&](Key k)
    {
        seen.insert(k);
        ++total;
    };

    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            add(piece_keys[p][s]);
    add(side_key);
    for (int i = 0; i < 16; ++i)
        add(castle_keys[i]);
    for (int i = 0; i < 8; ++i)
        add(en_passant_keys[i]);

    REQUIRE(seen.size() == total);
}

TEST_CASE("generate_key is consistent for identical positions", "[zobrist]")
{
    Position a = Position::from_fen(START_FEN);
    Position b = Position::from_fen(START_FEN);
    REQUIRE(Zobrist::generate_key(a) == Zobrist::generate_key(b));
    REQUIRE(a.key == b.key);
}

TEST_CASE("generate_key differs by side_key when only side differs", "[zobrist]")
{
    Position w = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    Position b = Position::from_fen("4k3/8/8/8/8/8/8/4K3 b - - 0 1");
    REQUIRE((w.key ^ b.key) == side_key);
}

TEST_CASE("generate_key differs by castle_keys delta when only rights differ", "[zobrist]")
{
    Position full = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w KQkq - 0 1");
    Position partial = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w Kq - 0 1");

    REQUIRE(full.castlingRights == 15);
    REQUIRE(partial.castlingRights == (1 | 8));
    REQUIRE((full.key ^ partial.key) == (castle_keys[full.castlingRights] ^ castle_keys[partial.castlingRights]));
}

TEST_CASE("generate_key differs by en_passant_keys delta when only EP differs", "[zobrist]")
{
    Position withEp = Position::from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    Position noEp = Position::from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
    REQUIRE((withEp.key ^ noEp.key) == en_passant_keys[3]); // d-file
}

TEST_CASE("generate_key differs by piece_keys delta when a piece is added", "[zobrist]")
{
    Position base = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    Position withKnight = base;
    set_piece(withKnight, WHITE_KNIGHT, D4);
    withKnight.key = Zobrist::generate_key(withKnight);

    REQUIRE((base.key ^ withKnight.key) == piece_keys[WHITE_KNIGHT][D4]);
}

TEST_CASE("generate_key on empty board equals castle_keys[0]", "[zobrist]")
{
    Position empty = Position::from_fen("8/8/8/8/8/8/8/8 w - - 0 1");
    REQUIRE(empty.castlingRights == 0);
    REQUIRE(empty.enPassantSquare == SQ_NONE);
    REQUIRE(empty.sideToMove == WHITE);
    REQUIRE(empty.key == castle_keys[0]);
}

TEST_CASE("generate_key changes with piece placement", "[zobrist]")
{
    Position start = Position::from_fen(START_FEN);
    Position afterE4 = Position::from_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    REQUIRE(start.key != afterE4.key);
}

// ============================================================================
// D. make_move / unmake_move
// ============================================================================

TEST_CASE("Quiet pawn move round-trips", "[position][make_move]")
{
    Position pos = Position::from_fen(START_FEN);
    Position before = pos;

    Move m(E2, E3, QUIET_MOVE);
    pos.make_move(m);

    REQUIRE(pos.board[E3] == WHITE_PAWN);
    REQUIRE(pos.board[E2] == PIECE_NONE);
    REQUIRE(pos.sideToMove == BLACK);
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.key == Zobrist::generate_key(pos));

    pos.unmake_move(m);
    REQUIRE(positions_equal(pos, before));
}

TEST_CASE("Double push sets EP only when a legal capturer exists", "[position][make_move]")
{
    SECTION("No enemy pawn adjacent -> EP not set")
    {
        // White pawn on e2, no black pawn on d4/f4.
        Position pos = Position::from_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
        Move m(E2, E4, DOUBLE_PAWN_PUSH);
        pos.make_move(m);

        REQUIRE(pos.board[E4] == WHITE_PAWN);
        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
    }

    SECTION("Black pawn on d4 -> EP set to e3")
    {
        Position pos = Position::from_fen("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
        Move m(E2, E4, DOUBLE_PAWN_PUSH);
        pos.make_move(m);

        REQUIRE(pos.board[E4] == WHITE_PAWN);
        REQUIRE(pos.enPassantSquare == E3);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
    }

    SECTION("Black pawn on f4 -> EP set to e3")
    {
        Position pos = Position::from_fen("4k3/8/8/8/5p2/8/4P3/4K3 w - - 0 1");
        pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
        REQUIRE(pos.enPassantSquare == E3);
    }

    SECTION("Black pawn on d5 does not create EP")
    {
        Position pos = Position::from_fen("4k3/8/8/3p4/8/8/4P3/4K3 w - - 0 1");
        pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
        REQUIRE(pos.enPassantSquare == SQ_NONE);
    }

    SECTION("Black double push with adjacent white pawn on d5")
    {
        Position pos = Position::from_fen("4k3/4p3/8/3P4/8/8/8/4K3 b - - 0 1");
        pos.make_move(Move(E7, E5, DOUBLE_PAWN_PUSH));
        REQUIRE(pos.enPassantSquare == E6);
    }
}

TEST_CASE("Quiet piece moves round-trip", "[position][make_move]")
{
    SECTION("Knight")
    {
        Position pos = Position::from_fen(START_FEN);
        Position before = pos;
        Move m(G1, F3, QUIET_MOVE);
        pos.make_move(m);
        REQUIRE(pos.board[F3] == WHITE_KNIGHT);
        REQUIRE(pos.board[G1] == PIECE_NONE);
        REQUIRE(pos.halfmoveClock == 1);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("Bishop")
    {
        Position pos = Position::from_fen("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
        Position before = pos;
        Move m(F1, C4, QUIET_MOVE);
        pos.make_move(m);
        REQUIRE(pos.board[C4] == WHITE_BISHOP);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("Rook from A1 forfeits Q only")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        Position before = pos;
        Move m(A1, A4, QUIET_MOVE);
        pos.make_move(m);
        REQUIRE(pos.board[A4] == WHITE_ROOK);
        REQUIRE(pos.castlingRights == 1); // K remains
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("Queen")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");
        Position before = pos;
        Move m(D1, D5, QUIET_MOVE);
        pos.make_move(m);
        REQUIRE(pos.board[D5] == WHITE_QUEEN);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("King forfeits both rights")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        Position before = pos;
        Move m(E1, E2, QUIET_MOVE);
        pos.make_move(m);
        REQUIRE(pos.board[E2] == WHITE_KING);
        REQUIRE(pos.castlingRights == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
}

TEST_CASE("Standard captures round-trip", "[position][make_move]")
{
    Position pos = Position::from_fen("4k3/8/8/8/3n4/8/2N5/4K3 w - - 0 5");
    Position before = pos;

    Move m(C2, D4, CAPTURE);
    pos.make_move(m);

    REQUIRE(pos.board[D4] == WHITE_KNIGHT);
    REQUIRE(pos.board[C2] == PIECE_NONE);
    REQUIRE((pos.pieces[BLACK][KNIGHT] & square_bb(D4)) == 0);
    REQUIRE((pos.occupancy[BLACK] & square_bb(D4)) == 0);
    REQUIRE((pos.occupancy[BOTH] & square_bb(D4)) != 0);
    REQUIRE(pos.halfmoveClock == 0);
    REQUIRE(pos.key == Zobrist::generate_key(pos));

    pos.unmake_move(m);
    REQUIRE(positions_equal(pos, before));
    REQUIRE(pos.board[D4] == BLACK_KNIGHT);
}

TEST_CASE("En passant captures round-trip for both colors", "[position][make_move]")
{
    SECTION("White captures black's EP pawn")
    {
        Position pos = Position::from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        Position before = pos;

        Move m(E5, D6, EP_CAPTURE);
        pos.make_move(m);

        REQUIRE(pos.board[D6] == WHITE_PAWN);
        REQUIRE(pos.board[E5] == PIECE_NONE);
        REQUIRE(pos.board[D5] == PIECE_NONE);
        REQUIRE((pos.pieces[BLACK][PAWN] & square_bb(D5)) == 0);
        REQUIRE(pos.halfmoveClock == 0);
        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
        REQUIRE(pos.board[D5] == BLACK_PAWN);
    }
    SECTION("Black captures white's EP pawn")
    {
        Position pos = Position::from_fen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
        Position before = pos;

        Move m(D4, E3, EP_CAPTURE);
        pos.make_move(m);

        REQUIRE(pos.board[E3] == BLACK_PAWN);
        REQUIRE(pos.board[D4] == PIECE_NONE);
        REQUIRE(pos.board[E4] == PIECE_NONE);
        REQUIRE((pos.pieces[WHITE][PAWN] & square_bb(E4)) == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
        REQUIRE(pos.board[E4] == WHITE_PAWN);
    }
}

TEST_CASE("Castling round-trips for both colors and both sides", "[position][make_move]")
{
    SECTION("White kingside")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        Position before = pos;
        Move m(E1, G1, KING_CASTLE);
        pos.make_move(m);
        REQUIRE(pos.board[G1] == WHITE_KING);
        REQUIRE(pos.board[F1] == WHITE_ROOK);
        REQUIRE(pos.board[E1] == PIECE_NONE);
        REQUIRE(pos.board[H1] == PIECE_NONE);
        REQUIRE(pos.castlingRights == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("White queenside")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        Position before = pos;
        Move m(E1, C1, QUEEN_CASTLE);
        pos.make_move(m);
        REQUIRE(pos.board[C1] == WHITE_KING);
        REQUIRE(pos.board[D1] == WHITE_ROOK);
        REQUIRE(pos.board[E1] == PIECE_NONE);
        REQUIRE(pos.board[A1] == PIECE_NONE);
        REQUIRE(pos.castlingRights == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("Black kingside")
    {
        Position pos = Position::from_fen("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1");
        Position before = pos;
        Move m(E8, G8, KING_CASTLE);
        pos.make_move(m);
        REQUIRE(pos.board[G8] == BLACK_KING);
        REQUIRE(pos.board[F8] == BLACK_ROOK);
        REQUIRE(pos.castlingRights == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("Black queenside")
    {
        Position pos = Position::from_fen("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1");
        Position before = pos;
        Move m(E8, C8, QUEEN_CASTLE);
        pos.make_move(m);
        REQUIRE(pos.board[C8] == BLACK_KING);
        REQUIRE(pos.board[D8] == BLACK_ROOK);
        REQUIRE(pos.castlingRights == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));
        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
}

TEST_CASE("Quiet promotions round-trip for all four pieces", "[position][make_move]")
{
    const std::string fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";

    struct Case
    {
        MoveFlag flag;
        Piece expected;
    };

    const Case cases[] = {
        {KNIGHT_PROMO, WHITE_KNIGHT},
        {BISHOP_PROMO, WHITE_BISHOP},
        {ROOK_PROMO, WHITE_ROOK},
        {QUEEN_PROMO, WHITE_QUEEN},
    };

    for (const auto& c : cases)
    {
        Position pos = Position::from_fen(fen);
        Position before = pos;
        Move m(A7, A8, c.flag);
        pos.make_move(m);

        REQUIRE(pos.board[A8] == c.expected);
        REQUIRE(pos.board[A7] == PIECE_NONE);
        REQUIRE((pos.pieces[WHITE][PAWN] & square_bb(A8)) == 0);
        REQUIRE(pos.halfmoveClock == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
    }
}

TEST_CASE("Promotion captures round-trip for all four pieces", "[position][make_move]")
{
    struct Case
    {
        MoveFlag flag;
        Piece expected;
    };

    const Case cases[] = {
        {KNIGHT_PROMO_CAP, WHITE_KNIGHT},
        {BISHOP_PROMO_CAP, WHITE_BISHOP},
        {ROOK_PROMO_CAP, WHITE_ROOK},
        {QUEEN_PROMO_CAP, WHITE_QUEEN},
    };

    for (const auto& c : cases)
    {
        Position pos = Position::from_fen("3n1k2/4P3/8/8/8/8/8/4K3 w - - 0 1");
        Position before = pos;
        Move m(E7, D8, c.flag);
        pos.make_move(m);

        REQUIRE(pos.board[D8] == c.expected);
        REQUIRE(pos.board[E7] == PIECE_NONE);
        REQUIRE((pos.pieces[BLACK][KNIGHT] & square_bb(D8)) == 0);
        REQUIRE(pos.halfmoveClock == 0);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(m);
        REQUIRE(positions_equal(pos, before));
        REQUIRE(pos.board[D8] == BLACK_KNIGHT);
    }
}

TEST_CASE("Null move preserves the board and round-trips", "[position][make_move]")
{
    SECTION("From a position with no en passant square")
    {
        Position pos = Position::from_fen(START_FEN);
        Position before = pos;

        Move z = Move::null();
        pos.make_move(z);

        for (auto sq : all_squares())
            REQUIRE(pos.board[sq] == before.board[sq]);
        REQUIRE(pos.sideToMove == BLACK);
        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.halfmoveClock == 1);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(z);
        REQUIRE(positions_equal(pos, before));
    }
    SECTION("From a position with an en passant square set")
    {
        Position pos = Position::from_fen("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
        pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
        REQUIRE(pos.enPassantSquare == E3);

        Position before = pos;
        Move z = Move::null();
        pos.make_move(z);

        REQUIRE(pos.enPassantSquare == SQ_NONE);
        REQUIRE(pos.key == Zobrist::generate_key(pos));

        pos.unmake_move(z);
        REQUIRE(positions_equal(pos, before));
        REQUIRE(pos.enPassantSquare == E3);
    }
}

TEST_CASE("Null move differs from original key by exactly side_key plus EP delta", "[position][make_move][zobrist]")
{
    // No EP set: delta should be exactly side_key.
    Position a = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    Key originalKey = a.key;
    a.make_move(Move::null());
    REQUIRE((originalKey ^ a.key) == side_key);

    // EP set: delta should be en_passant_keys[file] ^ side_key.
    Position b = Position::from_fen("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
    b.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
    Key epKey = b.key;
    b.make_move(Move::null());
    REQUIRE((epKey ^ b.key) == (en_passant_keys[file_of(E3)] ^ side_key));
}

// ============================================================================
// E. Castling rights edge cases
// ============================================================================

TEST_CASE("Castling rights are updated by the square-based mask", "[position][castling-rights]")
{
    SECTION("King move forfeits both rights")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        pos.make_move(Move(E1, E2, QUIET_MOVE));
        REQUIRE(pos.castlingRights == 0);
    }
    SECTION("Rook move from A1 forfeits Q only")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        pos.make_move(Move(A1, A5, QUIET_MOVE));
        REQUIRE(pos.castlingRights == 1);
    }
    SECTION("Rook move from H1 forfeits K only")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        pos.make_move(Move(H1, H5, QUIET_MOVE));
        REQUIRE(pos.castlingRights == 2);
    }
    SECTION("Black rook move from A8 forfeits q only")
    {
        Position pos = Position::from_fen("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1");
        pos.make_move(Move(A8, A5, QUIET_MOVE));
        REQUIRE(pos.castlingRights == 4);
    }
    SECTION("Black rook move from H8 forfeits k only")
    {
        Position pos = Position::from_fen("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1");
        pos.make_move(Move(H8, H5, QUIET_MOVE));
        REQUIRE(pos.castlingRights == 8);
    }
    SECTION("Rook captured on H1 forfeits K")
    {
        Position pos = Position::from_fen("4k3/8/8/8/7r/8/8/R3K2R b KQ - 0 1");
        pos.make_move(Move(H4, H1, CAPTURE));
        REQUIRE(pos.castlingRights == 2); // Q only
    }
    SECTION("Rook captured on A1 forfeits Q")
    {
        Position pos = Position::from_fen("4k3/8/8/8/1r6/8/8/R3K2R b KQ - 0 1");
        pos.make_move(Move(B4, A1, CAPTURE));
        REQUIRE(pos.castlingRights == 1); // K only
    }
    SECTION("Non-rook leaving a corner square still clears the right")
    {
        // Documents that the mask is indexed by square, not by piece.
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K2N w KQ - 0 1");
        pos.make_move(Move(H1, G3, QUIET_MOVE)); // knight leaves H1
        REQUIRE(pos.castlingRights == 2);        // K lost anyway
    }
}

// ============================================================================
// F. Halfmove clock
// ============================================================================

TEST_CASE("Halfmove clock resets on pawn moves and captures, increments otherwise", "[position][halfmove]")
{
    Position pos = Position::from_fen(START_FEN);
    const auto moves = opening_sequence();
    const uint16_t expected[] = {0, 0, 1, 2, 3, 0, 0, 0};

    REQUIRE(sizeof(expected) / sizeof(expected[0]) == moves.size());

    for (size_t i = 0; i < moves.size(); ++i)
    {
        pos.make_move(moves[i]);
        INFO("after half-move " << (i + 1));
        REQUIRE(pos.halfmoveClock == expected[i]);
    }
}

// ============================================================================
// G. En passant square management
// ============================================================================

TEST_CASE("En passant square is cleared by non-double-push moves", "[position][ep]")
{
    Position pos = Position::from_fen("4k3/8/8/8/3p4/8/4P3/4K3 w - - 0 1");
    pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
    REQUIRE(pos.enPassantSquare == E3);

    pos.make_move(Move(E8, E7, QUIET_MOVE));
    REQUIRE(pos.enPassantSquare == SQ_NONE);
}

TEST_CASE("EP square is not set if no capturer exists even after a double push", "[position][ep]")
{
    // Starting position: after 1.e4 no black pawn is on d4/f4.
    Position pos = Position::from_fen(START_FEN);
    pos.make_move(Move(E2, E4, DOUBLE_PAWN_PUSH));
    REQUIRE(pos.enPassantSquare == SQ_NONE);
    REQUIRE(pos.key == Zobrist::generate_key(pos));
}

// ============================================================================
// H. Zobrist consistency across sequences
// ============================================================================

TEST_CASE("Zobrist key matches generate_key after every move of a realistic sequence", "[position][zobrist]")
{
    Position pos = Position::from_fen(START_FEN);
    const auto moves = opening_sequence();

    for (size_t i = 0; i < moves.size(); ++i)
    {
        pos.make_move(moves[i]);
        INFO("after half-move " << (i + 1));
        REQUIRE(pos.key == Zobrist::generate_key(pos));
    }
}

TEST_CASE("Full make/unmake sequence restores the original position", "[position][make_move][unmake_move]")
{
    Position pos = Position::from_fen(START_FEN);
    Position original = pos;
    const auto moves = opening_sequence();

    REQUIRE(pos.ply == 0);

    for (size_t i = 0; i < moves.size(); ++i)
    {
        pos.make_move(moves[i]);
        REQUIRE(pos.ply == static_cast<int>(i + 1));
    }

    for (size_t i = moves.size(); i-- > 0;)
    {
        pos.unmake_move(moves[i]);
        REQUIRE(pos.ply == static_cast<int>(i));
    }

    REQUIRE(positions_equal(pos, original));
}

TEST_CASE("Transposition: same position, different move orders, identical key", "[position][zobrist][transposition]")
{
    Position a = Position::from_fen(START_FEN);
    Position b = Position::from_fen(START_FEN);

    a.make_move(Move(G1, F3, QUIET_MOVE));
    a.make_move(Move(B8, C6, QUIET_MOVE));
    a.make_move(Move(B1, C3, QUIET_MOVE));

    b.make_move(Move(B1, C3, QUIET_MOVE));
    b.make_move(Move(B8, C6, QUIET_MOVE));
    b.make_move(Move(G1, F3, QUIET_MOVE));

    REQUIRE(positions_equal(a, b));
    REQUIRE(a.key == b.key);
    REQUIRE(a.key == Zobrist::generate_key(a));
}

// ============================================================================
// I. is_square_attacked
// ============================================================================

TEST_CASE("is_square_attacked: pawns for both colors", "[position][attacks]")
{
    SECTION("White pawn on e4")
    {
        Position pos = Position::from_fen("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
        REQUIRE(pos.is_square_attacked(D5, WHITE));
        REQUIRE(pos.is_square_attacked(F5, WHITE));
        REQUIRE_FALSE(pos.is_square_attacked(E5, WHITE));
        REQUIRE_FALSE(pos.is_square_attacked(D3, WHITE));
    }
    SECTION("Black pawn on e5")
    {
        Position pos = Position::from_fen("4k3/8/8/4p3/8/8/8/4K3 b - - 0 1");
        REQUIRE(pos.is_square_attacked(D4, BLACK));
        REQUIRE(pos.is_square_attacked(F4, BLACK));
        REQUIRE_FALSE(pos.is_square_attacked(E4, BLACK));
    }
}

TEST_CASE("is_square_attacked: knight L-shape", "[position][attacks]")
{
    Position pos = Position::from_fen("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
    INFO("A1 = " << pos.board[A1] << "  D4 = " << pos.board[D4] << "  E1 = " << pos.board[E1]);
    INFO("white knights = 0x" << std::hex << pos.pieces[WHITE][KNIGHT]);
    const Square attacked[] = {B3, B5, C2, C6, E2, E6, F3, F5};
    for (Square sq : attacked)
    {
        INFO("target " << sq);
        REQUIRE(pos.is_square_attacked(sq, WHITE));
    }
    REQUIRE_FALSE(pos.is_square_attacked(D5, WHITE));
    REQUIRE_FALSE(pos.is_square_attacked(D4, WHITE));
}

TEST_CASE("is_square_attacked: king adjacent squares", "[position][attacks]")
{
    Position pos = Position::from_fen("8/8/8/3K4/8/8/8/4k3 w - - 0 1");
    const Square attacked[] = {C4, C5, C6, D4, D6, E4, E5, E6};
    for (Square sq : attacked)
        REQUIRE(pos.is_square_attacked(sq, WHITE));
    REQUIRE_FALSE(pos.is_square_attacked(D5, WHITE)); // own square
    REQUIRE_FALSE(pos.is_square_attacked(D7, WHITE));
}

TEST_CASE("is_square_attacked: bishop/queen diagonals stop at blockers", "[position][attacks]")
{
    SECTION("Clear diagonal")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/1B6/4K3 w - - 0 1");
        REQUIRE(pos.is_square_attacked(G7, WHITE));
        REQUIRE(pos.is_square_attacked(D4, WHITE));
    }
    SECTION("Blocked by a black pawn on d4")
    {
        Position pos = Position::from_fen("4k3/8/8/8/3p4/8/1B6/4K3 w - - 0 1");
        REQUIRE(pos.is_square_attacked(D4, WHITE)); // blocker is attacked
        REQUIRE_FALSE(pos.is_square_attacked(E5, WHITE));
        REQUIRE_FALSE(pos.is_square_attacked(G7, WHITE));
    }
}

TEST_CASE("is_square_attacked: rook/queen files stop at blockers", "[position][attacks]")
{
    SECTION("Clear file")
    {
        Position pos = Position::from_fen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
        REQUIRE(pos.is_square_attacked(A8, WHITE));
        REQUIRE(pos.is_square_attacked(A4, WHITE));
    }
    SECTION("Blocked by black pawn on a4")
    {
        Position pos = Position::from_fen("4k3/8/8/8/p7/8/8/R3K3 w - - 0 1");
        REQUIRE(pos.is_square_attacked(A4, WHITE));
        REQUIRE_FALSE(pos.is_square_attacked(A5, WHITE));
        REQUIRE_FALSE(pos.is_square_attacked(A8, WHITE));
    }
}

TEST_CASE("is_square_attacked: no attackers", "[position][attacks]")
{
    Position pos = Position::from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    REQUIRE_FALSE(pos.is_square_attacked(D4, WHITE));
    REQUIRE_FALSE(pos.is_square_attacked(D4, BLACK));
}

TEST_CASE("is_square_attacked: king in check", "[position][attacks]")
{
    Position pos = Position::from_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
    REQUIRE(pos.is_square_attacked(E1, BLACK));
}

TEST_CASE("is_square_attacked: own king on target square", "[position][attacks]")
{
    // Attackers_to own king works via the OR with square_bb(sq).
    Position pos = Position::from_fen("4k3/8/8/8/8/8/4q3/4K3 w - - 0 1");
    REQUIRE(pos.is_square_attacked(E1, BLACK));
    REQUIRE_FALSE(pos.is_square_attacked(E1, WHITE));
}