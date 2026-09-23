#include "UciEngine.hpp"

#include "Process.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
    bool parseInfoLine(const std::string& line, UciInfo& info)
    {
        if (!line.starts_with("info "))
            return false;

        info.raw = line;

        std::istringstream stream(line);
        std::string token;

        stream >> token; // "info"

        while (stream >> token)
        {
            if (token == "depth")
            {
                stream >> info.depth;
            }
            else if (token == "seldepth")
            {
                stream >> info.seldepth;
            }
            else if (token == "nodes")
            {
                stream >> info.nodes;
            }
            else if (token == "nps")
            {
                stream >> info.nps;
            }
            else if (token == "time")
            {
                stream >> info.timeMs;
            }
            else if (token == "score")
            {
                std::string scoreType;
                int score = 0;

                stream >> scoreType >> score;

                if (scoreType == "cp")
                {
                    info.scoreCp = score;
                }
                else if (scoreType == "mate")
                {
                    info.scoreMate = score;
                }
            }
            else if (token == "pv")
            {
                std::string move;

                while (stream >> move)
                    info.pv.push_back(std::move(move));

                break;
            }
            else
            {
                // Ignore UCI fields that the UI doesn't currently use.
            }
        }

        return true;
    }
} // namespace

struct UciEngine::Impl
{
    enum class State
    {
        Stopped,
        Starting,
        Ready,
        Searching
    };

    Process process;

    std::jthread readerThread;

    std::mutex queueMutex;
    std::queue<std::string> outputQueue;

    std::atomic<bool> running = false;

    State state = State::Stopped;

    void start(const std::filesystem::path& path);
    void startReaderThread();
    void writeLine(const std::string& line);
};

UciEngine::UciEngine()
    : impl_(std::make_unique<Impl>())
{
}

UciEngine::~UciEngine()
{
    stop();
}

void UciEngine::start(const std::filesystem::path& path)
{
    if (impl_->process.hasProcess())
        stop();

    impl_->start(path);
}

void UciEngine::Impl::start(const std::filesystem::path& path)
{
    {
        std::lock_guard lock(queueMutex);

        std::queue<std::string> empty;
        outputQueue.swap(empty);
    }

    state = State::Starting;

    try
    {
        process.start(path);

        running.store(true);

        startReaderThread();

        writeLine("uci");
    }
    catch (...)
    {
        process.terminate();

        if (readerThread.joinable())
        {
            readerThread.request_stop();
            readerThread.join();
        }

        process.reset();

        running.store(false);
        state = State::Stopped;

        throw;
    }
}

void UciEngine::Impl::startReaderThread()
{
    Impl* impl = this;
    Process* process = &this->process;

    // The reader thread owns the processing of raw engine output.
    // It only places complete lines into outputQueue.
    readerThread = std::jthread(
        [impl, process](std::stop_token stop)
        {
            char buffer[4096];
            std::string pending;

            while (!stop.stop_requested())
            {
                const std::ptrdiff_t bytesRead = process->read(buffer, sizeof(buffer));

                if (bytesRead <= 0)
                    break;

                pending.append(buffer, static_cast<std::size_t>(bytesRead));

                std::size_t lineStart = 0;

                while (true)
                {
                    const std::size_t newline = pending.find('\n', lineStart);

                    if (newline == std::string::npos)
                    {
                        break;
                    }

                    std::string line = pending.substr(lineStart, newline - lineStart);

                    // UCI engines normally use CRLF on Windows.
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back();
                    }

                    {
                        std::lock_guard lock(impl->queueMutex);

                        impl->outputQueue.push(std::move(line));
                    }

                    lineStart = newline + 1;
                }

                // Keep any incomplete final line for the next ReadFile().
                if (lineStart > 0)
                    pending.erase(0, lineStart);
            }

            // The engine may have closed stdout with a partial final line.
            if (!pending.empty())
            {
                std::lock_guard lock(impl->queueMutex);

                impl->outputQueue.push(std::move(pending));
            }

            impl->running.store(false);
        });
}

void UciEngine::Impl::writeLine(const std::string& line)
{
    const std::string data = line + '\n';

    if (!process.write(data))
    {
        throw std::runtime_error("Failed to write to UCI engine.");
    }
}

bool UciEngine::isRunning() const
{
    return impl_->running.load();
}

bool UciEngine::isReady() const
{
    return impl_->running.load() && impl_->state == Impl::State::Ready;
}

void UciEngine::stop()
{
    if (!impl_->process.hasProcess())
    {
        if (impl_->readerThread.joinable())
        {
            impl_->readerThread.request_stop();
            impl_->readerThread.join();
        }

        impl_->running.store(false);
        impl_->state = Impl::State::Stopped;

        return;
    }

    // Ask the engine to exit.
    try
    {
        impl_->writeLine("quit");
    }
    catch (...)
    {
        // Ignore write failures during shutdown.
    }

    // Wait for it to terminate.
    const bool exited = impl_->process.waitForExit(std::chrono::seconds(2));

    // Force termination if necessary.
    if (!exited)
        impl_->process.terminate();

    if (impl_->readerThread.joinable())
    {
        impl_->readerThread.request_stop();
        impl_->readerThread.join();
    }

    impl_->process.reset();

    impl_->running.store(false);
    impl_->state = Impl::State::Stopped;
}

void UciEngine::newGame()
{
    if (!isReady())
        return;

    impl_->writeLine("ucinewgame");
}

void UciEngine::stopSearch()
{
    if (impl_->state != Impl::State::Searching)
        return;

    impl_->writeLine("stop");
}

void UciEngine::setPosition(const std::string& fen, const std::vector<Move>& moves)
{
    if (!isReady())
        return;

    std::ostringstream command;

    if (fen.empty())
    {
        command << "position startpos";
    }
    else
    {
        command << "position fen " << fen;
    }

    if (!moves.empty())
    {
        command << " moves";

        for (const Move& move : moves)
        {
            command << ' ' << move;
        }
    }

    impl_->writeLine(command.str());
}

void UciEngine::go(const UciSearchLimits& limits)
{
    if (!isReady())
        return;

    std::ostringstream command;

    command << "go";

    if (limits.wtimeMs)
        command << " wtime " << *limits.wtimeMs;

    if (limits.btimeMs)
        command << " btime " << *limits.btimeMs;

    if (limits.wincMs)
        command << " winc " << *limits.wincMs;

    if (limits.bincMs)
        command << " binc " << *limits.bincMs;

    if (limits.movetimeMs)
        command << " movetime " << *limits.movetimeMs;

    if (limits.depth)
        command << " depth " << *limits.depth;

    if (limits.infinite)
        command << " infinite";

    impl_->writeLine(command.str());

    impl_->state = Impl::State::Searching;
}

UciPollResult UciEngine::poll()
{
    UciPollResult result;

    std::queue<std::string> pendingLines;

    {
        std::lock_guard lock(impl_->queueMutex);

        std::swap(pendingLines, impl_->outputQueue);
    }

    while (!pendingLines.empty())
    {
        std::string line = std::move(pendingLines.front());

        pendingLines.pop();

        if (line.starts_with("info "))
        {
            UciInfo info;

            if (parseInfoLine(line, info))
                result.infos.push_back(std::move(info));
        }
        else if (line.starts_with("bestmove "))
        {
            std::istringstream stream(line);

            std::string token;
            std::string bestMove;

            stream >> token; // "bestmove"
            stream >> bestMove;

            if (!bestMove.empty() && bestMove != "(none)")
            {
                result.bestMove = std::move(bestMove);
            }

            impl_->state = Impl::State::Ready;
        }

        if (line == "uciok")
        {
            if (impl_->running.load() && impl_->state == Impl::State::Starting)
            {
                try
                {
                    impl_->writeLine("isready");
                }
                catch (...)
                {
                    impl_->state = Impl::State::Stopped;
                }
            }
        }
        else if (line == "readyok")
        {
            if (impl_->running.load() && impl_->state == Impl::State::Starting)
            {
                impl_->state = Impl::State::Ready;
            }
        }

        // id ...
        // option ...
        // info string ...
        // etc.
    }

    if (!impl_->running.load())
        impl_->state = Impl::State::Stopped;

    return result;
}