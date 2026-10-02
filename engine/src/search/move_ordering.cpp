#include "move_ordering.h"
#include "heuristics.h"
#include "see.h"

namespace MoveOrdering
{
    const int MVV_LVA[PieceType::NONE][PieceType::NONE] = {{15, 14, 13, 12, 11, 10}, {25, 24, 23, 22, 21, 20},
                                                           {35, 34, 33, 32, 31, 30}, {45, 44, 43, 42, 41, 40},
                                                           {55, 54, 53, 52, 51, 50}, {0, 0, 0, 0, 0, 0}};

    MovePicker::MovePicker(const Position& p, Move tt, int pl, bool qs)
        : pos(p),
          tt_move(tt),
          ply(pl),
          qsearch(qs),
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
                move.set_score(TT_MOVE_SCORE);
                continue;
            }

            if (move.is_promotion())
            {
                PieceType pt = promotion_type(move);
                if (pt == QUEEN)
                {
                    const PieceType victim = type_of(pos.board[move.to()]);
                    const int16_t mvv_lva_bonus = (victim < PieceType::NONE) ? MVV_LVA[victim][PAWN] : 0;
                    move.set_score(static_cast<int16_t>(PROMOTION_QUEEN + mvv_lva_bonus));
                }
                else if (pt == KNIGHT)
                {
                    const PieceType victim = type_of(pos.board[move.to()]);
                    const int16_t mvv_lva_bonus = (victim < PieceType::NONE) ? MVV_LVA[victim][PAWN] : 0;
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

            if (attacker_type < PieceType::NONE && victim_type < PieceType::NONE)
            {
                if (Search::see_ge(pos, move, 0))
                {
                    move.set_score(static_cast<int16_t>(GOOD_CAPTURE + MVV_LVA[victim_type][attacker_type]));
                }
                else
                {
                    move.set_score(static_cast<int16_t>(BAD_CAPTURE + MVV_LVA[victim_type][attacker_type]));
                }
            }
            else
            {
                move.set_score(GOOD_CAPTURE);
            }
        }
    }

    void MovePicker::score_quiets()
    {
        for (int i = 0; i < list.count; ++i)
        {
            Move& move = list.moves[i];

            if (move.is_capture() || move.is_promotion() || move == tt_move)
            {
                if (!pos.in_check())
                {
                    move.set_score(-32000);
                }
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

            const Piece attacker = pos.board[move.from()];
            move.set_score(static_cast<int16_t>(Search::history_score(move, attacker)));
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
                    if (tt_move.is_ok())
                        return tt_move;
                    break;

                case STAGE_GEN_TACTICAL:
                    list.count = 0;
                    if (pos.in_check())
                    {
                        MoveGen::generate_pseudo_legal_moves(pos, list);
                        score_captures();
                        score_quiets();
                        stage = STAGE_QUIETS;
                    }
                    else
                    {
                        MoveGen::generate_captures(pos, list);
                        score_captures();
                        stage = STAGE_GOOD_CAPTURES;
                    }
                    break;

                case STAGE_GOOD_CAPTURES:
                    {
                        Move best = get_best_from_list();
                        if (!best.is_ok())
                        {
                            stage = qsearch ? STAGE_DONE : STAGE_GEN_QUIETS;
                            break;
                        }
                        if (best == tt_move)
                            continue;

                        if (best.ordering_score() >= BAD_CAPTURE)
                        {
                            return best;
                        }
                        else
                        {
                            bad_captures[bad_capture_count++] = best;
                        }
                        break;
                    }

                case STAGE_GEN_QUIETS:
                    list.count = 0;
                    MoveGen::generate_pseudo_legal_moves(pos, list);
                    score_quiets();
                    stage = STAGE_QUIETS;
                    break;

                case STAGE_QUIETS:
                    {
                        Move best = get_best_from_list();
                        if (!best.is_ok() || best.ordering_score() == -32000)
                        {
                            stage = STAGE_BAD_CAPTURES;
                            break;
                        }
                        if (best == tt_move)
                            continue;
                        return best;
                    }

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