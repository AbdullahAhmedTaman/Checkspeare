#include "uci.h"
#include "eval/evaluate.h"
#include "board/movegen.h"
#include "board/position.h"
#include "search/search.h"
#include "search/transposition.h"
#include "board/types.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace UCI
{
    Move string_to_move(const std::string& move_str, Position& pos)
    {
        MoveList legal_moves;
        MoveGen::generate_legal_moves(pos, legal_moves);

        for (int i = 0; i < legal_moves.count; ++i)
        {
            Move m = legal_moves.moves[i];
            std::stringstream ss;
            ss << m;
            if (ss.str() == move_str)
            {
                return m;
            }
        }
        return Move::none();
    }

    void send_search_info(const Search::SearchInfo& info) // NOTE: call it inside search loop!
    {
        uint64_t nps = (info.time_ms > 0) ? (info.nodes * 1000) / info.time_ms : 0;
        // clang-format off
        std::cout << "info depth " << info.depth 
                  << " seldepth " << info.seldepth 
                  << " nodes " << info.nodes 
                  << " nps " << nps
                  << " time " << info.time_ms 
                  << " score ";
        // clang-format on
        if (std::abs(info.score) > VALUE_MATE - MAX_PLY)
        {
            int moves_to_mate = (VALUE_MATE - std::abs(info.score) + 1) / 2;
            moves_to_mate = (info.score > 0) ? moves_to_mate : -moves_to_mate;
            std::cout << "mate " << moves_to_mate;
        }
        else
        {
            std::cout << "cp " << info.score;
        }

        std::cout << " pv";
        for (const Move& m : info.pv)
        {
            std::cout << " " << m;
        }

        std::cout << std::endl;
    }

    void loop()
    {
        Position pos;
        std::string line;
        std::thread search_thread;

        while (std::getline(std::cin, line))
        {
            std::istringstream iss(line);
            std::string command;
            iss >> command;

            if (command == "uci")
            {
                std::cout << "id name Check-Spear" << std::endl;
                std::cout << "id author Check-Mates" << std::endl;
                std::cout << "uciok" << std::endl;
            }
            else if (command == "isready")
            {
                std::cout << "readyok" << std::endl;
            }
            else if (command == "ucinewgame")
            {
                Search::init();
                Search::TT.clear();
            }
            else if (command == "position")
            {
                std::string token;
                iss >> token;

                if (token == "startpos")
                {
                    pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); //
                    iss >> token;
                }
                else if (token == "fen")
                {
                    std::string fen;
                    while (iss >> token && token != "moves")
                    {
                        fen += token + " ";
                    }
                    pos = Position::from_fen(fen);
                }

                Eval::init_scores(pos);

                if (token == "moves")
                {
                    while (iss >> token)
                    {
                        Move m = string_to_move(token, pos);
                        if (m.is_ok())
                        {
                            pos.make_move(m);
                        }
                    }
                }
            }
            else if (command == "go")
            {
                SearchLimits limits;
                std::string token;

                while (iss >> token)
                {
                    if (token == "wtime")
                        iss >> limits.wtime;
                    else if (token == "btime")
                        iss >> limits.btime;
                    else if (token == "winc")
                        iss >> limits.winc;
                    else if (token == "binc")
                        iss >> limits.binc;
                    else if (token == "depth")
                        iss >> limits.depth;
                    else if (token == "nodes")
                        iss >> limits.max_nodes;
                    else if (token == "movetime")
                        iss >> limits.movetime;
                    else if (token == "infinite")
                        limits.infinite = true;
                }

                if (search_thread.joinable())
                {
                    search_thread.join();
                }

                Search::Signals_stop = false;

                search_thread = std::thread(Search::start_search, std::ref(pos), limits);
            }
            else if (command == "stop")
            {
                Search::Signals_stop = true;
            }
            else if (command == "quit")
            {
                Search::Signals_stop = true;
                if (search_thread.joinable())
                {
                    search_thread.join();
                }
                break;
            }
        }
    }
} // namespace UCI