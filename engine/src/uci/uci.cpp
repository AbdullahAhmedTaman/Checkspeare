#include "uci.h"
#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "types.h"
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

            std::string current_move_str = "";
            current_move_str += char('a' + (m.from() % 8));
            current_move_str += char('1' + (m.from() / 8));
            current_move_str += char('a' + (m.to() % 8));
            current_move_str += char('1' + (m.to() / 8));

            if (m.is_promotion())
            {
                PieceType pt = promotion_type(m);
                if (pt == QUEEN)
                    current_move_str += 'q';
                else if (pt == ROOK)
                    current_move_str += 'r';
                else if (pt == BISHOP)
                    current_move_str += 'b';
                else if (pt == KNIGHT)
                    current_move_str += 'n';
            }

            if (move_str == current_move_str)
            {
                return m;
            }
        }
        return Move::none();
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
                std::cout << "id name dummyEngine" << std::endl;
                std::cout << "id author dummyTeam" << std::endl;
                std::cout << "uciok" << std::endl;
            }
            else if (command == "isready")
            {
                std::cout << "readyok" << std::endl;
            }
            else if (command == "ucinewgame")
            {
                Search::init();
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