#pragma once

#include "board/position.h"
#include "board/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Book
{

    // 16-byte packed entry as laid out in a Polyglot .bin file.
    // Stored in big-endian byte order.
#pragma pack(push, 1)

    struct RawEntry
    {
        uint64_t key;    // Zobrist hash of the position
        uint16_t move;   // Encoded move
        uint16_t weight; // Move weight
        uint32_t learn;  // Legacy field, unused
    };

#pragma pack(pop)

    static_assert(sizeof(RawEntry) == 16, "Polyglot entry must be 16 bytes");

    class OpeningBook
    {
    public:
        OpeningBook() = default;
        ~OpeningBook();

        OpeningBook(const OpeningBook&) = delete;
        OpeningBook& operator=(const OpeningBook&) = delete;

        bool load(const std::string& path);
        void unload();

        bool is_loaded() const
        {
            return data_ != nullptr;
        }

        // selection_mode: 0 = best move, 1 = weighted random (default).
        Move probe(Position& pos, int selection_mode = 1);

    private:
        const RawEntry* data_ = nullptr;
        std::size_t entry_count_ = 0;

#if defined(_WIN32)
        void* file_handle_ = nullptr;
        void* map_handle_ = nullptr;
#else
        int file_handle_ = -1;
        void* map_handle_ = nullptr;
#endif

        static uint64_t polyglot_key(const Position& pos);

        struct EntryRange
        {
            const RawEntry* begin;
            const RawEntry* end;
        };

        EntryRange find_entries(uint64_t key) const;

        Move decode_move(uint16_t raw_move, const Position& pos) const;
        Move select_best(const std::vector<const RawEntry*>& entries, const Position& pos) const;
        Move select_weighted(const std::vector<const RawEntry*>& entries, const Position& pos) const;
    };

} // namespace Book