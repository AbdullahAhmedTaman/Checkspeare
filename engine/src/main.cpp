// Console test/demo harness for bitboard.h.
//
// This intentionally never spells out an enumerator name like Square::E4 —
// it only relies on Square being constructible from a 0..63 index (which
// square_bb/pop_lsb already assume). That way it compiles against whatever
// naming convention your real Square enum uses.

#include <iostream>
#include <string>

#include "bitboard.h"
#include "types.h"

namespace
{
    // Builds a Square from 0-based file/rank (file 0..7 = a..h, rank 0..7 = 1..8).
    constexpr Square make_square(int file, int rank)
    {
        return static_cast<Square>(rank * 8 + file);
    }

    // Algebraic notation ("e4"), derived from rank_of/file_of rather than from
    // the enum's own names.
    std::string square_name(Square square)
    {
        return std::string(1, static_cast<char>('a' + file_of(square))) +
               std::string(1, static_cast<char>('1' + rank_of(square)));
    }

    // Prints a bitboard as an 8x8 board, rank 8 at the top (matches how a
    // chessboard is normally drawn).
    void print_bitboard(Bitboard bb, const std::string& label)
    {
        std::cout << label << '\n';
        for (int rank = 7; rank >= 0; --rank)
        {
            std::cout << (rank + 1) << " | ";
            for (int file = 0; file < 8; ++file)
                std::cout << (get_bit(bb, make_square(file, rank)) ? "1 " : ". ");
            std::cout << '\n';
        }
        std::cout << "    ----------------\n    a b c d e f g h\n\n";
    }

    void section(const std::string& title)
    {
        std::cout << "\n== " << title << " ==\n";
    }

} // namespace

int main()
{
    const Square a1 = make_square(0, 0);
    const Square e4 = make_square(4, 3);
    const Square h8 = make_square(7, 7);
    const Square a4 = make_square(0, 3);
    const Square h4 = make_square(7, 3);

    section("Square / rank / file basics");
    for (Square s : {a1, e4, h8})
        std::cout << square_name(s) << ": rank_of=" << rank_of(s) << " file_of=" << file_of(s) << '\n';

    section("set_bit / get_bit / clear_bit / toggle_bit");
    Bitboard bb = BB_EMPTY;
    std::cout << "initial get_bit(e4) = " << get_bit(bb, e4) << '\n';
    set_bit(bb, e4);
    std::cout << "after set_bit(e4):    get_bit(e4) = " << get_bit(bb, e4) << '\n';
    toggle_bit(bb, e4);
    std::cout << "after toggle_bit(e4): get_bit(e4) = " << get_bit(bb, e4) << '\n';
    toggle_bit(bb, e4);
    clear_bit(bb, e4);
    std::cout << "after clear_bit(e4):  get_bit(e4) = " << get_bit(bb, e4) << '\n';

    section("popcount / lsb / pop_lsb");
    bb = BB_EMPTY;
    set_bit(bb, a1);
    set_bit(bb, e4);
    set_bit(bb, h8);
    std::cout << "popcount = " << popcount(bb) << '\n';
    std::cout << "lsb      = " << square_name(lsb(bb)) << '\n';
    while (bb != BB_EMPTY)
        std::cout << "pop_lsb -> " << square_name(pop_lsb(bb)) << '\n';
    std::cout << "popcount after popping all = " << popcount(bb) << '\n';

    section("rank_bb / file_bb");
    print_bitboard(rank_bb(e4), "rank_bb(e4)");
    print_bitboard(file_bb(e4), "file_bb(e4)");

    section("diagonal_bb / anti_diagonal_bb");
    print_bitboard(diagonal_bb(e4), "diagonal_bb(e4)  [a1-h8 direction]");
    print_bitboard(anti_diagonal_bb(e4), "anti_diagonal_bb(e4)  [a8-h1 direction]");
    print_bitboard(diagonal_bb(a1), "diagonal_bb(a1)  (alone in the corner)");
    print_bitboard(anti_diagonal_bb(h8), "anti_diagonal_bb(h8)  (alone in the corner)");

    section("Directional shifts from a central square");
    Bitboard center = square_bb(e4);
    print_bitboard(center, "start: e4");
    print_bitboard(shift_north_east(center), "shift_north_east(e4)");
    print_bitboard(shift_north_west(center), "shift_north_west(e4)");
    print_bitboard(shift_south_east(center), "shift_south_east(e4)");
    print_bitboard(shift_south_west(center), "shift_south_west(e4)");
    print_bitboard(shift_east(center), "shift_east(e4)");
    print_bitboard(shift_west(center), "shift_west(e4)");

    section("Edge wraparound checks (all should print 1 / true)");
    std::cout << "shift_east(h4)       empty? " << (shift_east(square_bb(h4)) == BB_EMPTY) << '\n';
    std::cout << "shift_west(a4)       empty? " << (shift_west(square_bb(a4)) == BB_EMPTY) << '\n';
    std::cout << "shift_north_east(h4) empty? " << (shift_north_east(square_bb(h4)) == BB_EMPTY) << '\n';
    std::cout << "shift_north_west(a4) empty? " << (shift_north_west(square_bb(a4)) == BB_EMPTY) << '\n';
    std::cout << "shift_south_east(h4) empty? " << (shift_south_east(square_bb(h4)) == BB_EMPTY) << '\n';
    std::cout << "shift_south_west(a4) empty? " << (shift_south_west(square_bb(a4)) == BB_EMPTY) << '\n';

    return 0;
}