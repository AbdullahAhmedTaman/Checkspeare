#include "search.h"
#include "evaluate.h"
#include "heuristics.h"
#include "move_ordering.h"
#include "movegen.h"
#include "transposition.h"
#include <algorithm>
#include <cstring>

namespace Search
{
    Move killer_moves[MAX_PLY][2] = {};
    int history_table[MAX_PIECES][MAX_SQUARES] = {};
    std::atomic<bool> Signals_stop = false;
    SearchInfo CurrentInfo;

    namespace
    {
        bool legal_move(Position& pos, Move move)
        {
            const Color us = pos.sideToMove;
            pos.make_move(move);
            const bool legal = !pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1));
            pos.unmake_move(move);
            return legal;
        }

        bool in_check(const Position& pos)
        {
            const Color us = pos.sideToMove;
            return pos.is_square_attacked(pos.king_square(us), static_cast<Color>(us ^ 1));
        }

        /**
         * @brief Applies a simple null-move verification search.
         * @return True when the reduced null search still fails high.
         */
        bool null_move_cutoff(Position& pos, int depth, int ply, Value beta)
        {
            constexpr int NULL_REDUCTION = 2;
            pos.make_move(Move::null());
            const Value score = -negamax(pos, depth - 1 - NULL_REDUCTION, ply + 1, -beta, -beta + 1, false);
            pos.unmake_move(Move::null());
            return score >= beta;
        }
    } // namespace

    void init() // clearing the killer moves and history table before a new search
    {
        std::memset(killer_moves, 0, sizeof(killer_moves));
        std::memset(history_table, 0, sizeof(history_table));
        CurrentInfo = SearchInfo{};
        Signals_stop = false;
    }

    /**
     * @brief Searches progressively deeper and records the best root move.
     * @param pos Position to search.
     * @param limits Search depth and stop controls.
     */
    void start_search(Position& pos, const SearchLimits& limits)
    {
        init();
        TT.resize(16);

        const int max_depth = limits.depth > 0 ? limits.depth : 1;
        for (int depth = 1; depth <= max_depth && !Signals_stop; ++depth)
        {
            const Value score = negamax(pos, depth, 0, -VALUE_INFINITE, VALUE_INFINITE);
            CurrentInfo.depth = depth;
            CurrentInfo.score = score;
        }
    }

    /**
     * @brief Performs depth-limited negamax with alpha-beta pruning.
     *
     * TT bounds are consulted before move generation, while quiet cutoffs
     * update the killer and history heuristics for later sibling nodes.
     */
    Value negamax(Position& pos, int depth, int ply, Value alpha, Value beta, bool do_null)
    {
        ++CurrentInfo.nodes;
        CurrentInfo.seldepth = std::max(CurrentInfo.seldepth, ply);

        if (Signals_stop || ply >= MAX_SEARCH_PLY - 1)
            return Eval::evaluate(pos);

        if (depth <= 0)
            return quiescence(pos, ply, alpha, beta);

        const Value original_alpha = alpha;
        TTEntry entry;
        const bool has_entry = TT.probe(pos.key, ply, entry);
        const Move tt_move = has_entry ? entry.best_move : Move::none();
        if (has_entry && entry.depth >= depth)
        {
            if (entry.flag == TT_EXACT)
                return static_cast<Value>(entry.score);
            if (entry.flag == TT_LOWERBOUND)
                alpha = std::max(alpha, static_cast<Value>(entry.score));
            else
                beta = std::min(beta, static_cast<Value>(entry.score));
            if (alpha >= beta)
                return static_cast<Value>(entry.score);
        }

        if (do_null && depth >= 3 && !in_check(pos))
        {
            const bool has_non_pawn =
                (pos.occupancy[pos.sideToMove] & ~(pos.pieces[pos.sideToMove][PAWN] | pos.pieces[pos.sideToMove][KING])) != 0;
            if (has_non_pawn && null_move_cutoff(pos, depth, ply, beta))
                return beta;
        }

        MoveList list;
        MoveGen::generate_pseudo_legal_moves(pos, list);
        MoveOrdering::MovePicker picker(list, pos, tt_move, ply);
        Move best_move = Move::none();
        Value best_score = -VALUE_INFINITE;
        int legal_count = 0;
        int move_number = 0;

        while (true)
        {
            const Move move = picker.next_move();
            if (!move.is_ok())
                break;
            if (!legal_move(pos, move))
                continue;

            const Piece moving_piece = pos.board[move.from()];
            pos.make_move(move);
            ++move_number;
            const bool reduce = depth >= 3 && move_number > 3 && !move.is_capture() && !move.is_promotion();
            Value score = -negamax(pos, depth - (reduce ? 2 : 1), ply + 1, -beta, -alpha, true);
            if (reduce && score > alpha)
                score = -negamax(pos, depth - 1, ply + 1, -beta, -alpha, true);
            pos.unmake_move(move);
            ++legal_count;

            if (score > best_score)
            {
                best_score = score;
                best_move = move;
            }
            if (score > alpha)
                alpha = score;
            if (score >= beta)
            {
                if (!move.is_capture() && !move.is_promotion())
                {
                    update_killers(move, ply);
                    update_history(move, depth, moving_piece);
                }
                break;
            }
            if (!move.is_capture() && !move.is_promotion())
                penalize_history(move, depth, moving_piece);
        }

        if (legal_count == 0)
            return in_check(pos) ? -VALUE_MATE + ply : VALUE_ZERO;

        const TTFlag flag = best_score >= beta ? TT_LOWERBOUND : best_score <= original_alpha ? TT_UPPERBOUND : TT_EXACT;
        TT.store(pos.key, ply, depth, best_score, flag, best_move);
        if (ply == 0)
            CurrentInfo.best_move = best_move;
        return best_score;
    }
} // namespace Search