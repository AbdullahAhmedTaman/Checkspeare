#pragma once
#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"

namespace MoveOrdering
{
    constexpr int16_t TT_MOVE_SCORE = 32000;
    constexpr int16_t PROMOTION_QUEEN = 30000;
    constexpr int16_t GOOD_CAPTURE = 28000;
    constexpr int16_t KILLER_SCORE_1 = 24000;
    constexpr int16_t KILLER_SCORE_2 = 22000;
    constexpr int16_t COUNTER_MOVE_SCORE = 20000;
    constexpr int16_t BAD_CAPTURE = -10000; ///< Demoted below history-boosted quiets
    constexpr int16_t PROMOTION_KNIGHT = 17000;
    constexpr int16_t PROMOTION_MINOR = -20000;

    extern const int MVV_LVA[PieceType::NONE][PieceType::NONE];

    enum PickerStage
    {
        STAGE_TT,
        STAGE_GEN_TACTICAL,
        STAGE_GOOD_CAPTURES,
        STAGE_GEN_QUIETS,
        STAGE_QUIETS,
        STAGE_BAD_CAPTURES,
        STAGE_DONE
    };

    class MovePicker
    {
    public:
        /**
         * @brief A lazy state-machine move picker. Generates quiet moves only when necessary.
         */
        MovePicker(const Position& pos, Move tt_move, int ply, bool qsearch = false);
        Move next_move();

    private:
        const Position& pos;
        Move tt_move;
        int ply;
        bool qsearch;
        bool in_check;
        PickerStage stage;
        MoveList list;

        Move bad_captures[MAX_MOVES];
        int bad_capture_count = 0;
        int bad_capture_idx = 0;

        Move get_best_from_list();
        void score_captures();
        void score_quiets();
    };
} // namespace MoveOrdering