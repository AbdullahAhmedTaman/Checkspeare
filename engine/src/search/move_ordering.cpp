#include "move_ordering.h"
#include "heuristics.h"
#include "see.h"

namespace MoveOrdering
{
    const int MVV_LVA[PieceType::NONE][PieceType::NONE] = {{15, 14, 13, 12, 11, 10}, {25, 24, 23, 22, 21, 20},
                                                           {35, 34, 33, 32, 31, 30}, {45, 44, 43, 42, 41, 40},
                                                           {55, 54, 53, 52, 51, 50}, {0, 0, 0, 0, 0, 0}};

    namespace
    {
        constexpr int CAPTURE_PIECE_VALUE[PieceType::NONE + 1] = {100, 300, 300, 500, 900, 20000, 0};
    } // namespace
    MovePicker::MovePicker(const Position& p, Move tt, int pl, bool qs)
        : pos(p),
          tt_move(tt),
          ply(pl),
          qsearch(qs),
          in_check(qs && p.in_check()),
          stage(STAGE_TT)
    {
    }

    Move MovePicker::get_best_from_list()
    {
        if (list.count == 0)
            return Move::none();

        int best_idx = 0;
        for (int i = 1; i < list.count; ++i)
        {
            if (list.moves[i].ordering_score() > list.moves[best_idx].ordering_score())
            {
                best_idx = i;
            }
        }

        Move best_move = list.moves[best_idx];
        list.moves[best_idx] = list.moves[list.count - 1];
        list.count--;
        return best_move;
    }

    void MovePicker::score_captures()
    {
        for (int i = 0; i < list.count; ++i)
        {
            Move& move = list.moves[i];

            if (move == tt_move)
            {
                list.moves[i] = list.moves[--list.count];
                --i;
                continue;
            }

            if (move.is_promotion())
            {
                const PieceType pt = promotion_type(move);
                const PieceType victim = type_of(pos.board[move.to()]);
                const int16_t mvv_lva_bonus =
                    (victim < PieceType::NONE) ? static_cast<int16_t>(MVV_LVA[victim][PAWN]) : 0;

                if (pt == QUEEN)
                {
                    move.set_score(static_cast<int16_t>(PROMOTION_QUEEN + mvv_lva_bonus));
                }
                else if (pt == KNIGHT)
                {
                    move.set_score(static_cast<int16_t>(PROMOTION_KNIGHT + mvv_lva_bonus));
                }
                else
                {
                    move.set_score(PROMOTION_MINOR);
                }
                continue;
            }

            const Piece attacker = pos.board[move.from()];
            const PieceType attacker_type = type_of(attacker);
            const PieceType victim_type = move.is_ep() ? PAWN : type_of(pos.board[move.to()]);

            // Order captures strictly by MVV-LVA; defer SEE until the move is actually picked.
            if (attacker_type < PieceType::NONE && victim_type < PieceType::NONE)
            {
                move.set_score(static_cast<int16_t>(GOOD_CAPTURE + MVV_LVA[victim_type][attacker_type]));
            }
            else
            {
                move.set_score(GOOD_CAPTURE);
            }
        }
    }

   void MovePicker::score_quiets()
    {
        Move counter = Move::none();
        Piece prev_piece = PIECE_NONE;
        Square prev_to = SQ_NONE;

        if (ply > 0 && ply < MAX_PLY)
        {
            const Move prev_move = Search::path_moves[ply - 1];
            prev_piece = Search::path_pieces[ply - 1];
            if (prev_move.is_ok() && !prev_move.is_null())
            {
                prev_to = prev_move.to();
                counter = Search::counter_move(prev_piece, prev_to);
            }
        }

        for (int i = 0; i < list.count; ++i)
        {
            Move& move = list.moves[i];

            if (move == tt_move)
            {
                list.moves[i] = list.moves[--list.count];
                --i;
                continue;
            }

            if (ply >= 0 && ply < MAX_PLY)
            {
                if (Search::killer_moves[ply][0] == move)
                {
                    move.set_score(KILLER_SCORE_1);
                    continue;
                }
                if (Search::killer_moves[ply][1] == move)
                {
                    move.set_score(KILLER_SCORE_2);
                    continue;
                }
            }

            if (move == counter)
            {
                move.set_score(COUNTER_MOVE_SCORE);
                continue;
            }

            const Piece attacker = pos.board[move.from()];
            int hist = Search::history_score(move, attacker);
            if (prev_piece < PIECE_NONE && is_valid(prev_to))
            {
                hist = (hist + Search::continuation_history_score(prev_piece, prev_to, attacker, move.to())) / 2;
            }
            move.set_score(static_cast<int16_t>(hist));
        }
    }

   Move MovePicker::next_move()
    {
        while (stage != STAGE_DONE)
        {
            switch (stage)
            {
                case STAGE_TT:
                    stage = STAGE_GEN_TACTICAL;
                    if (tt_move.is_ok() && (!qsearch || in_check || tt_move.is_capture() || tt_move.is_promotion()))
                    {
                        return tt_move;
                    }
                    break;

                case STAGE_GEN_TACTICAL:
                    MoveGen::generate_captures(pos, list);
                    score_captures();
                    stage = STAGE_GOOD_CAPTURES;
                    break;

                case STAGE_GOOD_CAPTURES:
                    while (list.count > 0)
                    {
                        Move best = get_best_from_list();

                        if (best.is_promotion())
                        {
                            if (best.ordering_score() >= 0)
                            {
                                return best;
                            }
                            if (!qsearch || in_check)
                            {
                                bad_captures[bad_capture_count++] = best;
                            }
                            continue;
                        }

                       
                        const PieceType attacker_pt = type_of(pos.board[best.from()]);
                        const PieceType victim_pt = best.is_ep() ? PAWN : type_of(pos.board[best.to()]);

                        if (CAPTURE_PIECE_VALUE[victim_pt] >= CAPTURE_PIECE_VALUE[attacker_pt] ||
                            Search::see_ge(pos, best, 0))
                        {
                            return best;
                        }

                        // Losing capture (SEE < 0):
                        // - In Q-Search (not in check): prune it completely.
                        // - In Main Search: tag with BAD_CAPTURE and defer to STAGE_BAD_CAPTURES.
                        if (!qsearch || in_check)
                        {
                            best.set_score(BAD_CAPTURE);
                            bad_captures[bad_capture_count++] = best;
                        }
                    }

                    stage = (qsearch && !in_check) ? STAGE_DONE : STAGE_GEN_QUIETS;
                    break;

                case STAGE_GEN_QUIETS:
                    MoveGen::generate_quiets(pos, list);
                    score_quiets();
                    stage = STAGE_QUIETS;
                    break;

                case STAGE_QUIETS:
                    if (list.count > 0)
                    {
                        return get_best_from_list();
                    }
                    stage = STAGE_BAD_CAPTURES;
                    break;

                case STAGE_BAD_CAPTURES:
                    if (bad_capture_idx < bad_capture_count)
                    {
                        return bad_captures[bad_capture_idx++];
                    }
                    stage = STAGE_DONE;
                    break;

                case STAGE_DONE:
                    return Move::none();
            }
        }
        return Move::none();
    }
} // namespace MoveOrdering