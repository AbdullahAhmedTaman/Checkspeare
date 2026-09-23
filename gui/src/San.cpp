#include "San.hpp"

#include <algorithm> // std::any_of
#include <cstdlib>   // std::abs
#include <string>
#include <vector>

//  Local helpers (not visible outside this translation unit)
namespace
{

    /// Maps an engine PieceType to the single uppercase letter SAN uses.
    /// Pawns return '\0' because SAN omits the letter for pawn moves
    /// ("e4", not "Pe4"). The caller is expected to skip the letter when
    /// this returns '\0'.
    char pieceLetter(PieceType t)
    {
        switch (t)
        {
            case KNIGHT:
                return 'N';
            case BISHOP:
                return 'B';
            case ROOK:
                return 'R';
            case QUEEN:
                return 'Q';
            case KING:
                return 'K';
            case PAWN:
                return '\0';
            default:
                return '\0';
        }
    }

    /// File (column) of a square as an algebraic char: 'a'..'h'.
    /// Squares are stored 0..63 with 0 == a1, 7 == h1, 8 == a2, etc.,
    /// so square % 8 gives the file index and square / 8 gives the rank index.
    char fileChar(Square sq)
    {
        return static_cast<char>('a' + (static_cast<int>(sq) % 8));
    }

    /// Rank (row) of a square as an algebraic char: '1'..'8'.
    char rankChar(Square sq)
    {
        return static_cast<char>('1' + (static_cast<int>(sq) / 8));
    }

    /// True if `m` is a castling move.
    /// We detect it geometrically rather than relying on an engine flag:
    /// only a king move of exactly two files along the same rank can be
    /// castling, so this is unambiguous. If the engine's Move type ever
    /// gains an is_castle() helper, this can be replaced by it.
    bool isCastle(const Move& m)
    {
        const int from = static_cast<int>(m.from());
        const int to = static_cast<int>(m.to());
        const int fileDelta = (to % 8) - (from % 8);
        const bool sameRank = (from / 8) == (to / 8);
        return sameRank && std::abs(fileDelta) == 2;
    }

    /// Disambiguation decision for a non-pawn, non-king move.
    ///
    /// SAN requires disambiguation whenever *another* piece of the same type
    /// and color could legally move to the same destination. The algorithm
    /// then picks the shortest unambiguous discriminator:
    ///
    ///   1. If no other candidate shares the mover's file  -> use file.
    ///   2. Else if no other candidate shares the mover's rank -> use rank.
    ///   3. Else (multiple candidates, files and ranks collide) -> use full square.
    ///
    /// Only fields relevant to the selected strategy are set to true; the
    /// caller appends file/rank chars accordingly.
    struct Disambiguation
    {
        bool needed = false;
        bool useFile = false;
        bool useRank = false;
        bool useFull = false;
    };

    Disambiguation computeDisambiguation(const Board& board, Move m, PieceType type, Color side)
    {
        Disambiguation d;

        const Square from = m.from();
        const Square to = m.to();

        // Gather the origins of every *other* same-type, same-color piece
        // that could legally reach `to`.
        std::vector<Square> others;
        for (const Move& other : board.allLegalMoves())
        {
            if (other.to() != to)
                continue;
            if (other.from() == from)
                continue;

            const Piece pc = board.pieceAt(other.from());
            if (type_of(pc) != type)
                continue;
            if (color_of(pc) != side)
                continue;

            others.push_back(other.from());
        }

        if (others.empty())
            return d; // no ambiguity, no discriminator needed

        d.needed = true;

        // Does any other candidate sit on the same file as the mover?
        const bool fileCollides = std::any_of(others.begin(), others.end(),
                                              [from](Square s)
                                              {
                                                  return fileChar(s) == fileChar(from);
                                              });

        // Does any other candidate sit on the same rank as the mover?
        const bool rankCollides = std::any_of(others.begin(), others.end(),
                                              [from](Square s)
                                              {
                                                  return rankChar(s) == rankChar(from);
                                              });

        if (!fileCollides)
            d.useFile = true;
        else if (!rankCollides)
            d.useRank = true;
        else
            d.useFull = true;

        return d;
    }

} // namespace

//  Public entry point

/// Produces the Standard Algebraic Notation for `m` in the position `board`
/// (which must be the position *before* the move).
///
/// We take `Board` **by value** on purpose: SAN needs the position *after*
/// the move in order to append "+" or "#", and the function mutates its
/// local copy via Board::applyMove without the caller noticing. Copying a
/// Board is cheap (Position + a small MoveList).
std::string toSan(Board board, Move m)
{
    // Case 1: castling. SAN uses "O-O" / "O-O-O" and never names squares.
    // We still need the check/mate suffix, so we don't return immediately.

    if (isCastle(m))
    {
        std::string san = (fileChar(m.to()) == 'g') ? "O-O" : "O-O-O";

        board.applyMove(m); // mutate our local copy
        if (board.isCheckmate())
            san += '#';
        else if (board.inCheck())
            san += '+';

        return san;
    }

    // Inspect the position *before* the move to decide what we're writing.

    const Piece movingPiece = board.pieceAt(m.from());
    const PieceType type = type_of(movingPiece);
    const Color side = color_of(movingPiece);

    std::string san;

    // Case 2: pawn moves.
    //
    // Non-capture:  <file><rank>              e.g. "e4"
    // Capture:      <originFile>x<file><rank> e.g. "exd5"
    // Promotion:    append "=<letter>"        e.g. "e8=Q", "exd8=Q"
    //
    // Note: pawns are never disambiguated by file because SAN uses the
    // origin file *as part of the capture notation*, which is enough to
    // uniquely identify a pawn capture in every legal position.

    if (type == PAWN)
    {
        // Detect capture *before* applying the move. En passant is not
        // handled here (see "Not handled" section) because the captured
        // pawn is not on m.to(); a proper fix requires an engine-provided
        // is_en_passant() flag on Move.
        const bool isCapture = board.pieceAt(m.to()) != PIECE_NONE;

        if (isCapture)
        {
            san += fileChar(m.from()); // "e" in "exd5"
            san += 'x';
        }

        san += fileChar(m.to());
        san += rankChar(m.to());

        if (m.is_promotion())
        {
            san += '=';
            san += pieceLetter(promotion_type(m));
        }
    }

    // Case 3: non-pawn, non-castling moves.
    //
    // Layout: <letter><disambig?><x?><destFile><destRank>
    //   e.g.  "Nf3", "Nxf3", "Ngf3", "N1f3", "Ng1f3", "Qh4#"

    else
    {
        san += pieceLetter(type);

        // The king is unique, so it never needs disambiguation.
        if (type != KING)
        {
            const Disambiguation d = computeDisambiguation(board, m, type, side);
            if (d.useFile)
            {
                san += fileChar(m.from());
            }
            if (d.useRank)
            {
                san += rankChar(m.from());
            }
            if (d.useFull)
            {
                san += fileChar(m.from());
                san += rankChar(m.from());
            }
        }

        if (board.pieceAt(m.to()) != PIECE_NONE)
            san += 'x';

        san += fileChar(m.to());
        san += rankChar(m.to());
    }

    // Check / checkmate suffix. Applied last, uniformly, for all move types.
    // We must test checkmate *before* check, because mate implies check
    // and we'd otherwise append '+' to a mating move.

    board.applyMove(m);

    if (board.isCheckmate())
        san += '#';
    else if (board.inCheck())
        san += '+';

    return san;
}

// not handled : En passant