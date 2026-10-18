#include "uci.h"
#include "board/movegen.h"
#include "board/position.h"
#include "board/types.h"
#include "book/book.h"
#include "eval/evaluate.h"
#include "search/search.h"
#include "search/transposition.h"
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

    void send_search_info(const Search::SearchInfo& info)
    {
        uint64_t nps = (info.time_ms > 0) ? (info.nodes * 1000) / info.time_ms : 0;

        std::cout << "info depth " << info.depth << " seldepth " << info.seldepth << " nodes " << info.nodes << " nps " << nps
                  << " time " << info.time_ms << " score ";

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
        Position pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::string line;
        std::thread search_thread;

        Book::OpeningBook book;
        std::string book_filename = "book.bin";
        std::string book_mode = "Weighted";

        int tt_size_mb = 64;

        // Try loading default opening book quietly
        book.load(book_filename);

        auto stop_search = [&]()
        {
            Search::Signals_stop = true;
            if (search_thread.joinable())
            {
                search_thread.join();
            }
        };

        while (std::getline(std::cin, line))
        {
            std::istringstream iss(line);
            std::string command;
            iss >> command;

            if (command == "uci")
            {
                std::cout << "id name Checkspeare" << std::endl;
                std::cout << "id author Check-Mates" << std::endl;
                std::cout << "option name Hash type spin default 64 min 1 max 32768" << std::endl;
                std::cout << "option name BookFile type string default book.bin" << std::endl;
                std::cout << "option name BookMode type combo default Weighted var Weighted var Best" << std::endl;
                std::cout << "uciok" << std::endl;
            }
            else if (command == "isready")
            {
                std::cout << "readyok" << std::endl;
            }
            else if (command == "ucinewgame")
            {
                stop_search();
                Search::init();
                Search::TT.resize(tt_size_mb);
                Search::TT.clear();
            }
            else if (command == "setoption")
            {
                stop_search();
                std::string token, name, val_str;
                iss >> token;

                while (iss >> token && token != "value")
                {
                    if (!name.empty())
                        name += " ";
                    name += token;
                }
                while (iss >> token)
                {
                    if (!val_str.empty())
                        val_str += " ";
                    val_str += token;
                }

                if (name == "Hash")
                {
                    try
                    {
                        int mb = std::stoi(val_str);
                        if (mb < 1)
                            mb = 1;
                        tt_size_mb = mb;
                        Search::TT.resize(tt_size_mb);
                    }
                    catch (const std::exception&)
                    {
                        std::cout << "info string Invalid Hash value: " << val_str << std::endl;
                    }
                }
                else if (name == "BookFile")
                {
                    book_filename = val_str;
                    if (book.load(book_filename))
                        std::cout << "info string Opening book loaded successfully: " << book_filename << std::endl;
                    else
                        std::cout << "info string FAILED to load opening book: " << book_filename << std::endl;
                }
                else if (name == "BookMode")
                {
                    book_mode = val_str;
                }
            }
            else if (command == "position")
            {
                stop_search();
                std::string token;
                iss >> token;

                if (token == "startpos")
                {
                    pos = Position::from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                    iss >> token; // Attempt to read "moves" if present
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
                Eval::init_pawn_masks();

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
                stop_search();

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

                // Probe opening book safely if available
                if (book.is_loaded())
                {
                    const int mode = (book_mode == "Best") ? 0 : 1;
                    const Move book_move = book.probe(pos, mode);
                    if (book_move.is_ok())
                    {
                        std::cout << "bestmove " << book_move << std::endl;
                        continue;
                    }
                }

                Search::Signals_stop = false;
                search_thread = std::thread(Search::start_search, std::ref(pos), limits);
            }
            else if (command == "stop")
            {
                stop_search();
            }
            else if (command == "quit")
            {
                stop_search();
                break;
            }
        }

        // Clean cleanup on loop exit (e.g. unexpected EOF on std::cin)
        stop_search();
    }
} // namespace UCI