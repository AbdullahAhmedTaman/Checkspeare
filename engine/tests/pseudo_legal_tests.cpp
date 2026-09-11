#include <catch2/catch_test_macros.hpp>

#include "attacks.h"
#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "types.h"

#include <initializer_list>
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
        std::string boardPart;
        std::string side;
        std::string castling;
        std::string ep;

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

    void generate_pseudo(const Position& pos, MoveList& list)
    {
        MoveGen::generate_pseudo_legal_moves(pos, list);
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
} // namespace

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: initial position has 20 moves", "[movegen][pseudo]")
{
    Position pos = position_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveList list;
    generate_pseudo(pos, list);

    REQUIRE(list.count == 20);

    for (Square sq : {A2, B2, C2, D2, E2, F2, G2, H2})
        CHECK(count_from(list, sq) == 2);

    CHECK(count_from(list, B1) == 2);
    CHECK(count_from(list, G1) == 2);
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: pawn pushes and blocks", "[movegen][pseudo][pawn]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/8/8/8/8/8/4P3/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E2) == 2);
        CHECK(has_move(list, E2, E3, QUIET_MOVE));
        CHECK(has_move(list, E2, E4, DOUBLE_PAWN_PUSH));
    }

    {
        Position pos = position_from_fen("7k/8/8/8/8/4n3/4P3/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E2) == 0);
    }

    {
        Position pos = position_from_fen("7k/8/8/8/4n3/8/4P3/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E2) == 1);
        CHECK(has_move(list, E2, E3, QUIET_MOVE));
        CHECK_FALSE(has_move_from_to(list, E2, E4));
    }

    {
        Position pos = position_from_fen("7k/4p3/8/8/8/8/8/K7 b - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E7) == 2);
        CHECK(has_move(list, E7, E6, QUIET_MOVE));
        CHECK(has_move(list, E7, E5, DOUBLE_PAWN_PUSH));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: pawn promotions", "[movegen][pseudo][pawn][promotion]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/P7/8/8/8/8/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A7) == 4);
        CHECK(has_move(list, A7, A8, QUEEN_PROMO));
        CHECK(has_move(list, A7, A8, ROOK_PROMO));
        CHECK(has_move(list, A7, A8, BISHOP_PROMO));
        CHECK(has_move(list, A7, A8, KNIGHT_PROMO));
    }

    {
        Position pos = position_from_fen("1n5k/P7/8/8/8/8/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A7) == 8);
        CHECK(has_move(list, A7, B8, QUEEN_PROMO_CAP));
        CHECK(has_move(list, A7, B8, ROOK_PROMO_CAP));
        CHECK(has_move(list, A7, B8, BISHOP_PROMO_CAP));
        CHECK(has_move(list, A7, B8, KNIGHT_PROMO_CAP));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: pawn captures", "[movegen][pseudo][pawn]")
{
    Position pos = position_from_fen("7k/8/8/2p1p3/3P4/8/8/K7 w - - 0 1");
    MoveList list;
    generate_pseudo(pos, list);

    CHECK(count_from(list, D4) == 3);
    CHECK(has_move(list, D4, D5, QUIET_MOVE));
    CHECK(has_move(list, D4, C5, CAPTURE));
    CHECK(has_move(list, D4, E5, CAPTURE));
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: en passant", "[movegen][pseudo][pawn][ep]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E5) == 2);
        CHECK(has_move(list, E5, E6, QUIET_MOVE));
        CHECK(has_move(list, E5, D6, EP_CAPTURE));
    }

    {
        Position pos = position_from_fen("7k/8/8/3pP3/8/8/8/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E5) == 1);
        CHECK_FALSE(has_move_from_to(list, E5, D6));
    }

    {
        Position pos = position_from_fen("7k/8/8/8/3Pp3/8/8/K7 b - d3 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, E4) == 2);
        CHECK(has_move(list, E4, E3, QUIET_MOVE));
        CHECK(has_move(list, E4, D3, EP_CAPTURE));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: knight moves and edge squares", "[movegen][pseudo][knight]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/8/8/8/8/8/8/N6K w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 2);
        CHECK(has_move(list, A1, B3, QUIET_MOVE));
        CHECK(has_move(list, A1, C2, QUIET_MOVE));
    }

    {
        Position pos = position_from_fen("k6N/8/8/8/8/8/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, H8) == 2);
        CHECK(has_move(list, H8, F7, QUIET_MOVE));
        CHECK(has_move(list, H8, G6, QUIET_MOVE));
    }

    {
        Position pos = position_from_fen("7k/8/2P1p3/8/3N4/8/8/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, D4) == 7);
        CHECK(has_move(list, D4, E6, CAPTURE));
        CHECK_FALSE(has_move_from_to(list, D4, C6));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: rook moves and blocking", "[movegen][pseudo][rook]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/8/8/8/4K3/8/8/R7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 14);
    }

    {
        Position pos = position_from_fen("7k/8/8/8/4K3/p7/P7/R7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 7);
        CHECK_FALSE(has_move_from_to(list, A1, A2));
        CHECK_FALSE(has_move_from_to(list, A1, A3));
    }

    {
        Position pos = position_from_fen("7k/8/8/8/4K3/p7/8/R7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 9);
        CHECK(has_move(list, A1, A2, QUIET_MOVE));
        CHECK(has_move(list, A1, A3, CAPTURE));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: bishop moves", "[movegen][pseudo][bishop]")
{
    MoveList list;

    {
        Position pos = position_from_fen("8/7k/8/8/4K3/8/8/B7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 7);
    }

    {
        Position pos = position_from_fen("7k/8/8/8/3B4/8/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, D4) == 13);
    }

    {
        Position pos = position_from_fen("7k/8/8/8/3B4/2P5/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, D4) == 10);
        CHECK_FALSE(has_move_from_to(list, D4, C3));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: queen moves", "[movegen][pseudo][queen]")
{
    Position pos = position_from_fen("7k/8/8/8/3Q4/8/8/4K3 w - - 0 1");
    MoveList list;
    generate_pseudo(pos, list);

    CHECK(count_from(list, D4) == 27);
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: king moves", "[movegen][pseudo][king]")
{
    MoveList list;

    {
        Position pos = position_from_fen("7k/8/8/8/8/8/8/K7 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, A1) == 3);
    }

    {
        Position pos = position_from_fen("7k/8/8/8/2PKp3/8/8/8 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(count_from(list, D4) == 7);
        CHECK(has_move(list, D4, E4, CAPTURE));
        CHECK_FALSE(has_move_from_to(list, D4, C4));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: castling basic", "[movegen][pseudo][castling]")
{
    MoveList list;

    {
        Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        generate_pseudo(pos, list);

        CHECK(has_move(list, E1, G1, KING_CASTLE));
        CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
    }

    {
        Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3KB1R w KQkq - 0 1");
        generate_pseudo(pos, list);

        CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
        CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
    }

    {
        Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
        generate_pseudo(pos, list);

        CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
        CHECK_FALSE(has_move(list, E1, C1, QUEEN_CASTLE));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: castling cannot pass through or land on attacked squares",
                 "[movegen][pseudo][castling]")
{
    MoveList list;

    {
        // f1 attacked by rook f8 -> O-O illegal.
        Position pos = position_from_fen("r3kr2/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        generate_pseudo(pos, list);

        CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
        CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
    }

    {
        // e1 attacked by rook e8 -> both illegal.
        Position pos = position_from_fen("k3r3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        generate_pseudo(pos, list);

        CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
        CHECK_FALSE(has_move(list, E1, C1, QUEEN_CASTLE));
    }

    {
        // g1 attacked by rook g8 -> O-O illegal.
        Position pos = position_from_fen("k5r1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
        generate_pseudo(pos, list);

        CHECK_FALSE(has_move(list, E1, G1, KING_CASTLE));
        CHECK(has_move(list, E1, C1, QUEEN_CASTLE));
    }
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: black castling", "[movegen][pseudo][castling]")
{
    Position pos = position_from_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
    MoveList list;
    generate_pseudo(pos, list);

    CHECK(has_move(list, E8, G8, KING_CASTLE));
    CHECK(has_move(list, E8, C8, QUEEN_CASTLE));
}

TEST_CASE_METHOD(EngineFixture, "pseudo-legal: pinned pieces and king moves into check are still generated",
                 "[movegen][pseudo][pin]")
{
    MoveList list;

    {
        // White rook e2 is pinned by rook e8, but pseudo-legal still allows moving off the e-file.
        Position pos = position_from_fen("k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(has_move(list, E2, D2, QUIET_MOVE));
        CHECK(has_move(list, E2, E8, CAPTURE));
    }

    {
        // King move to e2 is illegal because rook e8 attacks e2, but pseudo-legal includes it.
        Position pos = position_from_fen("k3r3/8/8/8/8/8/8/4K3 w - - 0 1");
        generate_pseudo(pos, list);

        CHECK(has_move(list, E1, E2, QUIET_MOVE));
    }

    {
        // EP capture exposes king to rook e8, but pseudo-legal includes it.
        Position pos = position_from_fen("k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
        generate_pseudo(pos, list);

        CHECK(has_move(list, E5, D6, EP_CAPTURE));
    }
}