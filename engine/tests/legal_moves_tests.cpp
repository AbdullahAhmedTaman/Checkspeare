#include <catch2/catch_test_macros.hpp>

#include "attacks.h"
#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "types.h"

#include <sstream>
#include <string>

namespace
{
    void init_engine()
    {
        static bool initialized = false;
        if (!initialized)
        {
            init_attacks();
            initialized = true;
        }
    }

    struct EngineFixture
    {
        EngineFixture()
        {
            init_engine();
        }
    };

    Square make_square(int file, int rank)
    {
        return static_cast<Square>(rank * 8 + file);
    }

    Piece piece_from_char(char c)
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

    void put_piece(Position& pos, Square sq, Piece pc)
    {
        pos.board[sq] = pc;
        pos.pieces[color_of(pc)][type_of(pc)] |= square_bb(sq);
    }

    Position position_from_fen(const std::string& fen)
    {
        Position pos;
        pos.clear();

        std::istringstream ss(fen);
        std::string boardPart, side, castling, ep;
        ss >> boardPart >> side >> castling >> ep;

        int rank = 7;
        int file = 0;

        for (char c : boardPart)
        {
            if (c == '/')
            {
                --rank;
                file = 0;
            }
            else if (c >= '1' && c <= '8')
            {
                file += c - '0';
            }
            else
            {
                const Piece pc = piece_from_char(c);
                if (pc != PIECE_NONE)
                    put_piece(pos, make_square(file, rank), pc);
                ++file;
            }
        }

        pos.sideToMove = (side == "w") ? WHITE : BLACK;

        pos.castlingRights = 0;
        if (castling != "-")
        {
            for (char c : castling)
            {
                switch (c)
                {
                    case 'K':
                        pos.castlingRights |= 1;
                        break;
                    case 'Q':
                        pos.castlingRights |= 2;
                        break;
                    case 'k':
                        pos.castlingRights |= 4;
                        break;
                    case 'q':
                        pos.castlingRights |= 8;
                        break;
                    default:
                        break;
                }
            }
        }

        if (ep == "-")
        {
            pos.enPassantSquare = SQ_NONE;
        }
        else
        {
            const int f = ep[0] - 'a';
            const int r = ep[1] - '1';
            pos.enPassantSquare = make_square(f, r);
        }

        pos.update_occupancy();
        return pos;
    }

    void generate_legal(Position& pos, MoveList& list)
    {
        MoveGen::generate_legal_moves(pos, list);
    }

    int count_from(const MoveList& list, Square from)
    {
        int n = 0;
        for (int i = 0; i < list.count; ++i)
            if (list.moves[i].from() == from)
                ++n;
        return n;
    }

    bool has_move(const MoveList& list, Square from, Square to, MoveFlag flag)
    {
        const Move wanted(from, to, flag);
        for (int i = 0; i < list.count; ++i)
            if (list.moves[i] == wanted)
                return true;
        return false;
    }

    bool has_move_from_to(const MoveList& list, Square from, Square to)
    {
        for (int i = 0; i < list.count; ++i)
            if (list.moves[i].from() == from && list.moves[i].to() == to)
                return true;
        return false;
    }

    bool has_any_move_from(const MoveList& list, Square from)
    {
        for (int i = 0; i < list.count; ++i)
            if (list.moves[i].from() == from)
                return true;
        return false;
    }
} // namespace

// -----------------------------------------------------------------------------
// Baseline
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: initial position has exactly 20 moves", "[movegen][legal]")
{
    Position pos = position_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveList list;
    generate_legal(pos, list);

    REQUIRE(list.count == 20);
    CHECK(count_from(list, A2) == 2);
    CHECK(count_from(list, B1) == 2);
    CHECK(count_from(list, G1) == 2);
}

TEST_CASE_METHOD(EngineFixture, "legal: no restrictions means legal equals pseudo-legal count", "[movegen][legal]")
{
    // Quiet position, nothing pinned, no checks.
    Position pos = position_from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    MoveList pseudo, legal;
    MoveGen::generate_pseudo_legal_moves(pos, pseudo);
    MoveGen::generate_legal_moves(pos, legal);

    CHECK(pseudo.count == legal.count);
}

// -----------------------------------------------------------------------------
// Single checks and basic evasions
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: rook check, king must leave the file", "[movegen][legal][check]")
{
    // Rook e8 checks king e1 along the e-file.
    Position pos = position_from_fen("4r2k/8/8/8/8/8/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, E1, E2)); // still on e-file
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

TEST_CASE_METHOD(EngineFixture, "legal: bishop check, king must avoid the diagonal", "[movegen][legal][check]")
{
    // Bishop b4 checks along b4-c3-d2-e1.
    Position pos = position_from_fen("7k/8/8/8/1b6/8/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, E1, D2)); // on the diagonal
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, E2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

TEST_CASE_METHOD(EngineFixture, "legal: knight check cannot be blocked, only captured or dodged", "[movegen][legal][check]")
{
    // Knight d3 checks king e1.
    Position pos = position_from_fen("7k/8/8/8/8/3n4/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, E1, F2)); // attacked by knight
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, E2));
    CHECK(has_move_from_to(list, E1, F1));
}

TEST_CASE_METHOD(EngineFixture, "legal: pawn check, king can capture the pawn or flee", "[movegen][legal][check][pawn]")
{
    // Pawn d4 checks king e3 (pawn attacks c3 and e3).
    Position pos = position_from_fen("7k/8/8/8/3p4/4K3/8/8 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 8); // all 8 king neighbours are legal
    CHECK(has_move_from_to(list, E3, D4));
    CHECK(has_move_from_to(list, E3, D2));
    CHECK(has_move_from_to(list, E3, F2));
    CHECK(has_move_from_to(list, E3, F3));
    CHECK(has_move_from_to(list, E3, F4));
    CHECK(has_move_from_to(list, E3, E2));
    CHECK(has_move_from_to(list, E3, D3));
    CHECK(has_move_from_to(list, E3, E4));
}

TEST_CASE_METHOD(EngineFixture, "legal: only move is to block", "[movegen][legal][check][block]")
{
    // Rook h1 checks king d1 along rank 1. Bishop c3 can only block at e1.
    Position pos = position_from_fen("4k3/8/8/8/8/2B5/8/3K3r w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, D1, C1)); // on rank 1
    CHECK_FALSE(has_move_from_to(list, D1, E1)); // on rank 1
    CHECK(has_move_from_to(list, C3, E1));       // blocking
    CHECK(count_from(list, C3) == 1);
}

TEST_CASE_METHOD(EngineFixture, "legal: king captures the undefended checker", "[movegen][legal][check][capture]")
{
    // Rook d2 checks king e2. Rook is undefended, so Kxd2 is legal.
    Position pos = position_from_fen("7k/8/8/8/8/8/3rK3/8 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 5);
    CHECK(has_move_from_to(list, E2, D2));
    CHECK(has_move_from_to(list, E2, E1));
    CHECK(has_move_from_to(list, E2, E3));
    CHECK(has_move_from_to(list, E2, F1));
    CHECK(has_move_from_to(list, E2, F3));
    CHECK_FALSE(has_move_from_to(list, E2, D1)); // attacked by rook
    CHECK_FALSE(has_move_from_to(list, E2, D3)); // attacked by rook
    CHECK_FALSE(has_move_from_to(list, E2, F2)); // attacked by rook
}

TEST_CASE_METHOD(EngineFixture, "legal: king cannot capture a defended checker", "[movegen][legal][check][capture]")
{
    // Rook d2 checks king e2, defended by pawn c3. Kxd2 is illegal.
    Position pos = position_from_fen("7k/8/8/8/8/2p5/3rK3/8 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, E2, D2));
    CHECK_FALSE(has_move_from_to(list, E2, D1)); // attacked by rook
    CHECK_FALSE(has_move_from_to(list, E2, D3)); // attacked by rook
    CHECK_FALSE(has_move_from_to(list, E2, F2)); // attacked by rook
    CHECK(has_move_from_to(list, E2, E1));
    CHECK(has_move_from_to(list, E2, E3));
    CHECK(has_move_from_to(list, E2, F1));
    CHECK(has_move_from_to(list, E2, F3));
}

TEST_CASE_METHOD(EngineFixture, "legal: capture the checker with another piece", "[movegen][legal][check]")
{
    // Knight d3 checks king e1. Knights b4 and c5 can both capture it.
    Position pos = position_from_fen("7k/8/8/2N5/1N6/3n4/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 6);
    CHECK(has_move(list, C5, D3, CAPTURE));
    CHECK(has_move(list, B4, D3, CAPTURE));
    CHECK_FALSE(has_move_from_to(list, E1, F2)); // attacked by knight
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, E2));
    CHECK(has_move_from_to(list, E1, F1));
}

// -----------------------------------------------------------------------------
// Double check
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: double check forces king moves only", "[movegen][legal][check][double]")
{
    // Bishop b4 and knight c2 both give check.
    Position pos = position_from_fen("7k/8/8/8/1b6/8/2n5/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 4);
    CHECK_FALSE(has_move_from_to(list, E1, D2)); // attacked by bishop
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, E2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

// -----------------------------------------------------------------------------
// Pins
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: knight pinned by rook cannot move at all", "[movegen][legal][pin]")
{
    // Knight e2 pinned by rook e8.
    Position pos = position_from_fen("4r2k/8/8/8/8/8/4N3/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(count_from(list, E2) == 0);
    CHECK(list.count == 4); // king only
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

TEST_CASE_METHOD(EngineFixture, "legal: knight pinned by bishop cannot move", "[movegen][legal][pin]")
{
    // Knight b2 pinned by bishop c3 along a1-c3 diagonal.
    Position pos = position_from_fen("7k/8/8/8/8/2b5/1N6/K7 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(count_from(list, B2) == 0);
    CHECK(list.count == 2);
    CHECK(has_move_from_to(list, A1, A2));
    CHECK(has_move_from_to(list, A1, B1));
    CHECK_FALSE(has_move_from_to(list, A1, B2)); // own knight
}

TEST_CASE_METHOD(EngineFixture, "legal: pinned rook may move along the pin line and capture the pinner", "[movegen][legal][pin]")
{
    // Rook e2 pinned by rook e8, but may slide along the e-file and capture.
    Position pos = position_from_fen("4r2k/8/8/8/8/8/4R3/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 10);
    CHECK(count_from(list, E2) == 6);
    CHECK(has_move(list, E2, E8, CAPTURE));
    CHECK_FALSE(has_move_from_to(list, E2, D2));
    CHECK_FALSE(has_move_from_to(list, E2, F2));
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

TEST_CASE_METHOD(EngineFixture, "legal: pinned pawn may push and capture along the file", "[movegen][legal][pin][pawn]")
{
    // White pawn e2 pinned by rook e8, but is free to push along e-file.
    Position pos = position_from_fen("4r2k/8/8/8/8/8/4P3/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, E2, E3, QUIET_MOVE));
    CHECK(has_move(list, E2, E4, DOUBLE_PAWN_PUSH));
    // No sideways pawn captures exist here, but ensure nothing bogus.
    CHECK(count_from(list, E2) == 2);
}

// -----------------------------------------------------------------------------
// En passant edge cases
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: en passant that exposes king to rook is illegal", "[movegen][legal][ep]")
{
    Position pos = position_from_fen("4r2k/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    MoveList list;
    generate_legal(pos, list);

    // EP opens the e-file -> illegal.
    CHECK_FALSE(has_move(list, E5, D6, EP_CAPTURE));

    // Push is fine because e6 still blocks the rook.
    CHECK(has_move(list, E5, E6, QUIET_MOVE));

    // The pawn on e5 blocks the rook, so e2 is safe.
    CHECK(count_from(list, E1) == 5);
    CHECK(has_move_from_to(list, E1, E2));
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
    CHECK(list.count == 6);
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant that exposes king to bishop is illegal", "[movegen][legal][ep]")
{
    Position pos = position_from_fen("b6k/8/8/3pP3/8/8/8/7K w - d6 0 1");
    MoveList list;
    generate_legal(pos, list);

    // EP removes the d5 blocker on the a8-h1 diagonal -> illegal.
    CHECK_FALSE(has_move(list, E5, D6, EP_CAPTURE));

    CHECK(has_move(list, E5, E6, QUIET_MOVE));

    // Currently d5 still blocks the bishop, so g2 is safe.
    CHECK(has_move_from_to(list, H1, G2));
    CHECK(has_move_from_to(list, H1, G1));
    CHECK(has_move_from_to(list, H1, H2));
    CHECK(list.count == 4);
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant is legal when it does not expose the king", "[movegen][legal][ep]")
{
    Position pos = position_from_fen("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, E5, D6, EP_CAPTURE));
    CHECK(has_move(list, E5, E6, QUIET_MOVE));
    CHECK(count_from(list, A1) == 3);
    CHECK(list.count == 5);
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant can resolve a pawn check", "[movegen][legal][ep][check]")
{
    // Pawn d5 checks king e4. EP c5xd6 captures the pawn and resolves the check.
    Position pos = position_from_fen("7k/8/8/2Pp4/4K3/8/8/8 w - d6 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, C5, D6, EP_CAPTURE));
    CHECK(has_move(list, C5, C6, QUIET_MOVE) == false); // push does not resolve the check
    CHECK(count_from(list, E4) == 8);                   // all 8 king moves are available
    CHECK(list.count == 9);
}

TEST_CASE_METHOD(EngineFixture, "legal: EP target g6 does not resolve a d5 pawn check", "[movegen][legal][ep][check]")
{
    // Pawn d5 gives check to king e4. EP target is g6 (black just played g7-g5).
    // f5xg6 EP does not resolve the check; c5 has no legal move at all.
    Position pos = position_from_fen("7k/8/8/2Pp1Pp1/4K3/8/8/8 w - g6 0 1");
    MoveList list;
    generate_legal(pos, list);

    // The only EP available (f5xg6) does not address the check.
    CHECK_FALSE(has_move(list, F5, G6, EP_CAPTURE));
    CHECK_FALSE(has_move(list, F5, F6, QUIET_MOVE));
    CHECK_FALSE(has_move(list, C5, C6, QUIET_MOVE));

    // Only the king can move, and it must capture the checker or dodge.
    CHECK(count_from(list, E4) == 6);
    CHECK(has_move(list, E4, D5, CAPTURE));
    CHECK(has_move_from_to(list, E4, D3));
    CHECK(has_move_from_to(list, E4, D4));
    CHECK(has_move_from_to(list, E4, E3));
    CHECK(has_move_from_to(list, E4, E5));
    CHECK(has_move_from_to(list, E4, F3));
    CHECK_FALSE(has_move_from_to(list, E4, F4)); // attacked by g5 pawn
    CHECK_FALSE(has_move_from_to(list, E4, F5)); // own pawn

    CHECK(list.count == 6);
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant is the only move that resolves a pawn check", "[movegen][legal][ep][check]")
{
    // Black's last move was b7-b5, so the EP square is b6.
    // The b5 pawn gives check to the white king on a4.
    // Every king escape is blocked by a white piece, and b5 is defended by the
    // c6 pawn (so Kxb5 is illegal). No other white piece can capture b5 or
    // block the check. Therefore the only legal move is a5xb6 en passant.
    Position pos = position_from_fen("7k/8/2p5/Pp6/KN6/PP6/8/8 w - b6 0 1");

    MoveList list;
    generate_legal(pos, list);

    REQUIRE(list.count == 1);
    CHECK(has_move(list, A5, B6, EP_CAPTURE));

    // Sanity: these are all rejected.
    CHECK_FALSE(has_move_from_to(list, A5, A6)); // push doesn't stop the check
    CHECK_FALSE(has_move_from_to(list, A4, B5)); // Kxb5, defended by c6
    CHECK_FALSE(has_move_from_to(list, A4, A3)); // own pawn
    CHECK_FALSE(has_move_from_to(list, A4, A5)); // own pawn
    CHECK_FALSE(has_move_from_to(list, A4, B3)); // own pawn
    CHECK_FALSE(has_move_from_to(list, A4, B4)); // own knight
    CHECK(count_from(list, B4) == 0);            // knight cannot resolve the check
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant is the only move that blocks a bishop check", "[movegen][legal][ep][check]")
{
    // Black bishop a1 checks the white king on h8 along the a1-h8 diagonal.
    // Black just played f7-f5, so the EP target is f6.
    //
    // Nothing else works:
    //   Kg7  -> stays on the diagonal
    //   Kg8  -> attacked by the knight e7
    //   Kh7  -> own pawn
    //   g5-g6, h7-h8=* -> do not block the diagonal
    //   h7xg8 -> no piece there (knight is on e7)
    // Only g5xf6 EP interposes on the diagonal at f6.
    Position pos = position_from_fen("k6K/4n2P/8/5pP1/8/8/8/b7 w - f6 0 1");

    MoveList list;
    generate_legal(pos, list);

    REQUIRE(list.count == 1);
    CHECK(has_move(list, G5, F6, EP_CAPTURE));

    // Sanity checks.
    CHECK_FALSE(has_move_from_to(list, H8, G7));
    CHECK_FALSE(has_move_from_to(list, H8, G8));
    CHECK_FALSE(has_move_from_to(list, H8, H7));
    CHECK_FALSE(has_move_from_to(list, G5, G6));
    CHECK_FALSE(has_move_from_to(list, G5, H6));
    CHECK(count_from(list, G5) == 1); // only the EP
    CHECK(count_from(list, H7) == 0); // pawn has no moves at all
    CHECK(count_from(list, H8) == 0); // king has no escapes
}

// -----------------------------------------------------------------------------
// Castling edge cases
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: both castles available in a clean position", "[movegen][legal][castling]")
{
    Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, E1, G1, KING_CASTLE));
    CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "legal: cannot castle through an attacked square", "[movegen][legal][castling]")
{
    // Rook f8 attacks f1.
    Position pos = position_from_fen("r3kr2/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
    CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "legal: cannot castle out of check", "[movegen][legal][castling]")
{
    // Rook e8 checks king e1.
    Position pos = position_from_fen("k3r3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
    CHECK_FALSE(has_move(list, E1, C1, QUEEN_CASTLE));
    // Only king escapes off the e-file.
    CHECK(list.count == 4);
    CHECK(has_move_from_to(list, E1, D1));
    CHECK(has_move_from_to(list, E1, D2));
    CHECK(has_move_from_to(list, E1, F1));
    CHECK(has_move_from_to(list, E1, F2));
}

TEST_CASE_METHOD(EngineFixture, "legal: cannot castle into check", "[movegen][legal][castling]")
{
    // Rook g8 attacks g1.
    Position pos = position_from_fen("k5r1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
    CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "legal: attacked rook does not prevent castling", "[movegen][legal][castling]")
{
    // Bishop a8 attacks h1 (rook square) but not f1 or g1.
    Position pos = position_from_fen("b3k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, E1, G1, KING_CASTLE));
    CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "legal: black castling with an attack on f8 is blocked", "[movegen][legal][castling]")
{
    // White rook f1 attacks f8.
    Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3KR2 b Qkq - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move(list, E8, G8, KING_CASTLE));
    CHECK(has_move(list, E8, C8, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "legal: no castle without rights", "[movegen][legal][castling]")
{
    Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
    CHECK_FALSE(has_move(list, E1, C1, QUEEN_CASTLE));
}

// -----------------------------------------------------------------------------
// King safety & king adjacency
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: king cannot move adjacent to enemy king", "[movegen][legal][king]")
{
    Position pos = position_from_fen("8/8/4k3/8/4K3/8/8/8 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 5);
    CHECK_FALSE(has_move_from_to(list, E4, D5)); // adjacent to Ke6
    CHECK_FALSE(has_move_from_to(list, E4, E5)); // adjacent to Ke6
    CHECK_FALSE(has_move_from_to(list, E4, F5)); // adjacent to Ke6
    CHECK(has_move_from_to(list, E4, D3));
    CHECK(has_move_from_to(list, E4, D4));
    CHECK(has_move_from_to(list, E4, E3));
    CHECK(has_move_from_to(list, E4, F3));
    CHECK(has_move_from_to(list, E4, F4));
}

TEST_CASE_METHOD(EngineFixture, "legal: king captures undefended knight", "[movegen][legal][king]")
{
    Position pos = position_from_fen("8/8/8/4n3/4K3/8/8/7k w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move_from_to(list, E4, E5)); // captures knight
    CHECK(count_from(list, E4) == 6);      // d3 and f3 attacked by the knight
    CHECK_FALSE(has_move_from_to(list, E4, D3));
    CHECK_FALSE(has_move_from_to(list, E4, F3));
}

TEST_CASE_METHOD(EngineFixture, "legal: king cannot capture defended knight", "[movegen][legal][king]")
{
    Position pos = position_from_fen("8/8/5p2/4n3/4K3/8/8/7k w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK_FALSE(has_move_from_to(list, E4, E5)); // knight defended by f6 pawn
    CHECK(count_from(list, E4) == 5);
}

// -----------------------------------------------------------------------------
// Promotions resolving / causing check
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: promotion capture resolves a check", "[movegen][legal][promotion][check]")
{
    // Rook a8 checks king a1. Pawn b7 may capture-promote on a8.
    Position pos = position_from_fen("r6k/1P6/8/8/8/8/8/K7 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 6);
    CHECK(has_move(list, B7, A8, QUEEN_PROMO_CAP));
    CHECK(has_move(list, B7, A8, ROOK_PROMO_CAP));
    CHECK(has_move(list, B7, A8, BISHOP_PROMO_CAP));
    CHECK(has_move(list, B7, A8, KNIGHT_PROMO_CAP));
    CHECK_FALSE(has_move(list, B7, B8, QUEEN_PROMO)); // does not resolve the check
    CHECK(has_move_from_to(list, A1, B1));
    CHECK(has_move_from_to(list, A1, B2));
    CHECK_FALSE(has_move_from_to(list, A1, A2)); // still on the a-file
}

TEST_CASE_METHOD(EngineFixture, "legal: promotion moves are all generated when legal", "[movegen][legal][promotion]")
{
    Position pos = position_from_fen("7k/P7/8/8/8/8/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, A7, A8, QUEEN_PROMO));
    CHECK(has_move(list, A7, A8, ROOK_PROMO));
    CHECK(has_move(list, A7, A8, BISHOP_PROMO));
    CHECK(has_move(list, A7, A8, KNIGHT_PROMO));
    CHECK(count_from(list, A7) == 4);
}

// -----------------------------------------------------------------------------
// Stalemate and checkmate
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: stalemate produces zero legal moves", "[movegen][legal][stalemate]")
{
    // Black king a8 has no legal moves and is not in check.
    Position pos = position_from_fen("k7/2K5/1Q6/8/8/8/8/8 b - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 0);
}

TEST_CASE_METHOD(EngineFixture, "legal: back-rank mate produces zero legal moves", "[movegen][legal][checkmate]")
{
    Position pos = position_from_fen("4R1k1/5ppp/8/8/8/8/8/6K1 b - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 0);
}

TEST_CASE_METHOD(EngineFixture, "legal: mate in the corner with queen and king", "[movegen][legal][checkmate]")
{
    // King h8 mated by queen g7 supported by king f6.
    Position pos = position_from_fen("7k/6Q1/5K2/8/8/8/8/8 b - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(list.count == 0);
}

// -----------------------------------------------------------------------------
// Mixed / interaction cases
// -----------------------------------------------------------------------------

TEST_CASE_METHOD(EngineFixture, "legal: only one move escapes check", "[movegen][legal][check]")
{
    // Rook a8 checks king a1; only king b1 escapes (b2 also but see below).
    // White pawn b2 blocks b2, so Kb2 is blocked; only Kb1 remains.
    Position pos = position_from_fen("r6k/8/8/8/8/8/1P6/K7 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move_from_to(list, A1, B1));
    CHECK_FALSE(has_move_from_to(list, A1, A2)); // rook on a-file
    CHECK_FALSE(has_move_from_to(list, A1, B2)); // own pawn
}

TEST_CASE_METHOD(EngineFixture, "legal: two different pieces can resolve check", "[movegen][legal][check]")
{
    // Knight d3 checks king e1. King can also be captured by Nb4xd3 or Nc5xd3.
    // The knight f4 can block? No - knight checks can't be blocked. Instead, both
    // other knights capture, doubling the escape count.
    Position pos = position_from_fen("7k/8/8/2N5/1N6/3n4/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    int resolvers = 0;
    if (has_move(list, C5, D3, CAPTURE))
        ++resolvers;
    if (has_move(list, B4, D3, CAPTURE))
        ++resolvers;
    CHECK(resolvers == 2);
}

TEST_CASE_METHOD(EngineFixture, "legal: pinned pawn cannot push sideways but can capture the pinner on file",
                 "[movegen][legal][pin][pawn]")
{
    // Pawn e3 is pinned by rook e8. It may push forward but not capture diagonally off the e-file.
    // Black knight on d4 sits on the diagonal, but capturing it would expose the king.
    Position pos = position_from_fen("4r2k/8/8/8/3n4/4P3/8/4K3 w - - 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, E3, E4, QUIET_MOVE));
    CHECK_FALSE(has_move(list, E3, D4, CAPTURE)); // pinned
}

TEST_CASE_METHOD(EngineFixture, "legal: en passant legal even when the moved pawn was the only e-file blocker for another pawn",
                 "[movegen][legal][ep]")
{
    // EP on the queenside; nothing pins the capturing pawn.
    Position pos = position_from_fen("7k/8/8/2pP4/8/8/8/K7 w - c6 0 1");
    MoveList list;
    generate_legal(pos, list);

    CHECK(has_move(list, D5, C6, EP_CAPTURE));
    CHECK(has_move(list, D5, D6, QUIET_MOVE));
}

TEST_CASE_METHOD(EngineFixture, "legal: all moves are pseudo-legal moves that pass the check test", "[movegen][legal][sanity]")
{
    // A busy middlegame-like position: every legal move must be in the pseudo-legal list.
    Position pos = position_from_fen("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R1BQK2R w KQkq - 0 1");

    MoveList pseudo, legal;
    MoveGen::generate_pseudo_legal_moves(pos, pseudo);
    MoveGen::generate_legal_moves(pos, legal);

    REQUIRE(legal.count <= pseudo.count);
    for (int i = 0; i < legal.count; ++i)
    {
        bool found = false;
        for (int j = 0; j < pseudo.count; ++j)
        {
            if (pseudo.moves[j] == legal.moves[i])
            {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}