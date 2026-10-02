#include "book.h"
#include "polyglot_random.h"

#include "board/attacks.h"
#include "board/bitboard.h"
#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <random>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Book
{

    // Big-endian conversion helpers for Polyglot binary formats
    static inline uint64_t be64(uint64_t v) noexcept
    {
        if constexpr (std::endian::native == std::endian::big)
            return v;
        else
            return std::byteswap(v);
    }

    static inline uint16_t be16(uint16_t v) noexcept
    {
        if constexpr (std::endian::native == std::endian::big)
            return v;
        else
            return std::byteswap(v);
    }

    // Computes the standard Polyglot Zobrist hash key for the given position
    uint64_t OpeningBook::polyglot_key(const Position& pos)
    {
        uint64_t key = 0;

        // Piece placement (indices 0..767)
        for (int sq = 0; sq < 64; ++sq)
        {
            const Piece pc = pos.board[sq];
            if (pc == PIECE_NONE)
                continue;

            const int poly_piece = 2 * static_cast<int>(type_of(pc)) + (color_of(pc) == WHITE ? 1 : 0);
            key ^= POLYGLOT_RANDOM_ARRAY[poly_piece * 64 + sq];
        }

        // Castling rights (indices 768..771)
        if (pos.castlingRights & 1)
            key ^= POLYGLOT_RANDOM_ARRAY[768];
        if (pos.castlingRights & 2)
            key ^= POLYGLOT_RANDOM_ARRAY[769];
        if (pos.castlingRights & 4)
            key ^= POLYGLOT_RANDOM_ARRAY[770];
        if (pos.castlingRights & 8)
            key ^= POLYGLOT_RANDOM_ARRAY[771];

        // En passant target square (indices 772..779)
        if (pos.enPassantSquare != SQ_NONE)
        {
            const Color us = pos.sideToMove;
            const Color them = static_cast<Color>(us ^ 1);
            const Bitboard ep_capturers = pawn_attacks(them, pos.enPassantSquare) & pos.pieces[us][PAWN];

            if (ep_capturers)
            {
                key ^= POLYGLOT_RANDOM_ARRAY[772 + file_of(pos.enPassantSquare)];
            }
        }

        // Active color (index 780)
        if (pos.sideToMove == WHITE)
            key ^= POLYGLOT_RANDOM_ARRAY[780];

        return key;
    }

    // Binary searches the mapped binary file for matching Polyglot entries
    OpeningBook::EntryRange OpeningBook::find_entries(uint64_t key) const
    {
        if (data_ == nullptr || entry_count_ == 0)
            return {data_, data_};

        struct KeyComp
        {
            bool operator()(const RawEntry& entry, uint64_t k) const
            {
                return be64(entry.key) < k;
            }

            bool operator()(uint64_t k, const RawEntry& entry) const
            {
                return k < be64(entry.key);
            }
        };

        const RawEntry* first = data_;
        const RawEntry* last = data_ + entry_count_;

        const RawEntry* lo = std::lower_bound(first, last, key, KeyComp{});
        const RawEntry* hi = std::upper_bound(lo, last, key, KeyComp{});

        return {lo, hi};
    }

    // Decodes a raw Polyglot 16-bit move into internal Move representation
    Move OpeningBook::decode_move(uint16_t raw_move, const Position& pos) const
    {
        const uint16_t decoded_raw = be16(raw_move);

        const Square to = static_cast<Square>(decoded_raw & 0x3F);
        const Square from = static_cast<Square>((decoded_raw >> 6) & 0x3F);
        const int promo = static_cast<int>((decoded_raw >> 12) & 0x7);

        const Piece moving = pos.board[from];
        const Piece captured = pos.board[to];
        const PieceType pt = type_of(moving);

        const int df = file_of(to) - file_of(from);
        const int dr = rank_of(to) - rank_of(from);

        MoveFlag flag = QUIET_MOVE;

        if (promo != 0)
        {
            const bool is_cap = (captured != PIECE_NONE);
            switch (promo)
            {
                case 1:
                    flag = is_cap ? KNIGHT_PROMO_CAP : KNIGHT_PROMO;
                    break;
                case 2:
                    flag = is_cap ? BISHOP_PROMO_CAP : BISHOP_PROMO;
                    break;
                case 3:
                    flag = is_cap ? ROOK_PROMO_CAP : ROOK_PROMO;
                    break;
                case 4:
                    flag = is_cap ? QUEEN_PROMO_CAP : QUEEN_PROMO;
                    break;
                default:
                    return Move::none();
            }
        }
        else if (pt == KING && (df == 2 || df == -2))
        {
            flag = (df > 0) ? KING_CASTLE : QUEEN_CASTLE;
        }
        else if (pt == PAWN && df != 0 && captured == PIECE_NONE)
        {
            flag = EP_CAPTURE;
        }
        else if (pt == PAWN && (dr == 2 || dr == -2))
        {
            flag = DOUBLE_PAWN_PUSH;
        }
        else if (captured != PIECE_NONE)
        {
            flag = CAPTURE;
        }

        return Move(from, to, flag);
    }

    // Memory maps the book file into memory
    bool OpeningBook::load(const std::string& path)
    {
        unload();

#if defined(_WIN32)
        HANDLE hFile =
            CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER li;
        if (!GetFileSizeEx(hFile, &li))
        {
            CloseHandle(hFile);
            return false;
        }

        const std::size_t file_size = static_cast<std::size_t>(li.QuadPart);
        if (file_size == 0 || file_size % 16 != 0)
        {
            CloseHandle(hFile);
            return false;
        }

        HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap)
        {
            CloseHandle(hFile);
            return false;
        }

        void* ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (!ptr)
        {
            CloseHandle(hMap);
            CloseHandle(hFile);
            return false;
        }

        file_handle_ = hFile;
        map_handle_ = hMap;
#else
        const int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
            return false;

        struct stat st;
        if (fstat(fd, &st) != 0)
        {
            close(fd);
            return false;
        }

        const std::size_t file_size = static_cast<std::size_t>(st.st_size);
        if (file_size == 0 || file_size % 16 != 0)
        {
            close(fd);
            return false;
        }

        void* ptr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (ptr == MAP_FAILED)
        {
            close(fd);
            return false;
        }

        file_handle_ = fd;
        map_handle_ = ptr;
#endif

        data_ = reinterpret_cast<const RawEntry*>(ptr);
        entry_count_ = file_size / 16;
        return true;
    }

    // Unmaps the book file and releases handle allocations
    void OpeningBook::unload()
    {
        if (!data_)
            return;

#if defined(_WIN32)
        UnmapViewOfFile(data_);
        CloseHandle(reinterpret_cast<HANDLE>(map_handle_));
        CloseHandle(reinterpret_cast<HANDLE>(file_handle_));
#else
        munmap(map_handle_, entry_count_ * 16);
        close(file_handle_);
#endif

        data_ = nullptr;
        entry_count_ = 0;
        map_handle_ = nullptr;
#if defined(_WIN32)
        file_handle_ = nullptr;
#else
        file_handle_ = -1;
#endif
    }

    // Selects the book move with the highest weight
    Move OpeningBook::select_best(const std::vector<const RawEntry*>& entries, const Position& pos) const
    {
        const RawEntry* best = nullptr;
        uint16_t best_w = 0;

        for (const RawEntry* e : entries)
        {
            const uint16_t w = be16(e->weight);
            if (w > best_w)
            {
                best_w = w;
                best = e;
            }
        }

        if (best == nullptr)
            return Move::none();

        return decode_move(best->move, pos);
    }

    // Selects a move probabilistically based on entry weight distributions
    Move OpeningBook::select_weighted(const std::vector<const RawEntry*>& entries, const Position& pos) const
    {
        uint32_t total = 0;
        for (const RawEntry* e : entries)
            total += be16(e->weight);

        if (total == 0)
            return Move::none();

        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint32_t> dist(0, total - 1);
        const uint32_t pick = dist(rng);

        uint32_t cumulative = 0;
        for (const RawEntry* e : entries)
        {
            cumulative += be16(e->weight);
            if (pick < cumulative)
                return decode_move(e->move, pos);
        }

        return Move::none();
    }

    // Probes the opening book for a valid move for the current position
    Move OpeningBook::probe(Position& pos, int selection_mode)
    {
        if (!is_loaded())
            return Move::none();

        const uint64_t key = polyglot_key(pos);
        const EntryRange range = find_entries(key);

        if (range.begin == range.end)
            return Move::none();

        std::vector<const RawEntry*> entries;
        entries.reserve(static_cast<std::size_t>(range.end - range.begin));
        for (const RawEntry* p = range.begin; p != range.end; ++p)
            entries.push_back(p);

        Move chosen = (selection_mode == 0) ? select_best(entries, pos) : select_weighted(entries, pos);

        if (!chosen.is_ok())
            return Move::none();

        MoveList legal;
        MoveGen::generate_legal_moves(pos, legal);
        for (int i = 0; i < legal.count; ++i)
        {
            if (legal.moves[i] == chosen)
                return chosen;
        }

        return Move::none();
    }

    OpeningBook::~OpeningBook()
    {
        unload();
    }
} // namespace Book