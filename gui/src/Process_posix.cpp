#ifndef _WIN32

#include "Process.hpp"

#include <cerrno>
#include <csignal>
#include <mutex>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace
{
    void closeFd(int& fd) noexcept
    {
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
    }

    void ignoreSigpipe()
    {
        static std::once_flag flag;

        std::call_once(flag,
                       []
                       {
                           struct sigaction action{};

                           action.sa_handler = SIG_IGN;

                           sigemptyset(&action.sa_mask);

                           if (sigaction(SIGPIPE, &action, nullptr) != 0)
                           {
                               throw std::system_error(errno, std::generic_category(), "Failed to configure SIGPIPE handling.");
                           }
                       });
    }
} // namespace

Process::~Process() noexcept
{
    reset();
}

void Process::start(const std::filesystem::path& executable)
{
    if (executable.empty())
    {
        throw std::invalid_argument("UCI engine executable path is empty.");
    }

    reset();

    int stdinPipe[2] = {-1, -1};
    int stdoutPipe[2] = {-1, -1};

    // Pipe: GUI -> Engine
    //
    // stdinPipe[0] is the child's read end.
    // stdinPipe[1] is the parent's write end.
    if (pipe(stdinPipe) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to create stdin pipe.");
    }

    // Pipe: Engine -> GUI
    //
    // stdoutPipe[0] is the parent's read end.
    // stdoutPipe[1] is the child's write end.
    if (pipe(stdoutPipe) != 0)
    {
        closeFd(stdinPipe[0]);
        closeFd(stdinPipe[1]);

        throw std::system_error(errno, std::generic_category(), "Failed to create stdout pipe.");
    }

    posix_spawn_file_actions_t actions{};

    int result = posix_spawn_file_actions_init(&actions);

    if (result != 0)
    {
        closeFd(stdinPipe[0]);
        closeFd(stdinPipe[1]);
        closeFd(stdoutPipe[0]);
        closeFd(stdoutPipe[1]);

        throw std::system_error(result, std::generic_category(), "Failed to initialize process spawn actions.");
    }

    auto fail = [&](const char* message) -> void
    {
        posix_spawn_file_actions_destroy(&actions);

        closeFd(stdinPipe[0]);
        closeFd(stdinPipe[1]);
        closeFd(stdoutPipe[0]);
        closeFd(stdoutPipe[1]);

        throw std::system_error(result, std::generic_category(), message);
    };

    // Connect the child's stdin to the stdin pipe.
    result = posix_spawn_file_actions_adddup2(&actions, stdinPipe[0], STDIN_FILENO);
    if (result != 0)
        fail("Failed to configure child stdin.");

    // Connect the child's stdout to the stdout pipe.
    result = posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);
    if (result != 0)
        fail("Failed to configure child stdout.");

    // Match the Windows implementation: stderr goes to the same pipe as stdout.
    result = posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDERR_FILENO);
    if (result != 0)
        fail("Failed to configure child stderr.");

    // The child does not need the parent's stdin write end.
    if (stdinPipe[1] != STDIN_FILENO && stdinPipe[1] != STDOUT_FILENO && stdinPipe[1] != STDERR_FILENO)
    {
        result = posix_spawn_file_actions_addclose(&actions, stdinPipe[1]);
        if (result != 0)
            fail("Failed to configure child stdin pipe.");
    }

    // The child does not need the parent's stdout read end.
    if (stdoutPipe[0] != STDIN_FILENO && stdoutPipe[0] != STDOUT_FILENO && stdoutPipe[0] != STDERR_FILENO)
    {
        result = posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
        if (result != 0)
            fail("Failed to configure child stdout pipe.");
    }

    // The child's stdin read end was dup2'd onto STDIN_FILENO, so the
    // original fd is a redundant copy that must not leak into the child.
    if (stdinPipe[0] != STDIN_FILENO && stdinPipe[0] != STDOUT_FILENO && stdinPipe[0] != STDERR_FILENO)
    {
        result = posix_spawn_file_actions_addclose(&actions, stdinPipe[0]);
        if (result != 0)
            fail("Failed to configure child stdin read end.");
    }

    // The child's stdout write end was dup2'd onto STDOUT_FILENO and
    // STDERR_FILENO, so the original fd is a redundant copy.
    if (stdoutPipe[1] != STDIN_FILENO && stdoutPipe[1] != STDOUT_FILENO && stdoutPipe[1] != STDERR_FILENO)
    {
        result = posix_spawn_file_actions_addclose(&actions, stdoutPipe[1]);
        if (result != 0)
            fail("Failed to configure child stdout write end.");
    }

    const char* executablePath = executable.c_str();

    char* argv[] = {const_cast<char*>(executablePath), nullptr};

    pid_t processId = -1;

    result = posix_spawn(&processId, executablePath, &actions, nullptr, argv, environ);

    posix_spawn_file_actions_destroy(&actions);

    if (result != 0)
    {
        closeFd(stdinPipe[0]);
        closeFd(stdinPipe[1]);
        closeFd(stdoutPipe[0]);
        closeFd(stdoutPipe[1]);

        throw std::system_error(result, std::generic_category(), "Failed to create UCI engine process.");
    }

    // Parent no longer needs the child's copies.
    closeFd(stdinPipe[0]);
    closeFd(stdoutPipe[1]);

    processId_ = processId;
    stdinWrite_ = stdinPipe[1];
    stdoutRead_ = stdoutPipe[0];

    stdinPipe[1] = -1;
    stdoutPipe[0] = -1;
}

bool Process::write(std::string_view data)
{
    if (stdinWrite_ == -1)
        return false;

    // Prevent a failed write from terminating the entire GUI
    // with SIGPIPE when the engine has already exited.
    ignoreSigpipe();

    const char* current = data.data();
    std::size_t remaining = data.size();

    while (remaining > 0)
    {
        const ssize_t bytesWritten = ::write(stdinWrite_, current, remaining);

        if (bytesWritten > 0)
        {
            current += bytesWritten;
            remaining -= static_cast<std::size_t>(bytesWritten);
            continue;
        }

        if (bytesWritten < 0 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

std::ptrdiff_t Process::read(char* buffer, std::size_t size)
{
    if (stdoutRead_ == -1 || buffer == nullptr)
        return -1;

    if (size == 0)
        return 0;

    while (true)
    {
        const ssize_t result = ::read(stdoutRead_, buffer, size);

        if (result < 0 && errno == EINTR)
            continue;

        return static_cast<std::ptrdiff_t>(result);
    }
}

bool Process::waitForExit(std::chrono::milliseconds timeout) noexcept
{
    if (processId_ == -1)
        return true;

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true)
    {
        int status = 0;

        const pid_t result = waitpid(processId_, &status, WNOHANG);

        if (result == processId_)
        {
            processId_ = -1;
            return true;
        }

        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            return false;
        }

        if (timeout.count() <= 0)
            return false;

        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Process::terminate() noexcept
{
    if (processId_ == -1)
        return;

    kill(processId_, SIGKILL);

    int status = 0;

    while (true)
    {
        const pid_t result = waitpid(processId_, &status, 0);

        if (result == processId_)
            break;

        if (result == -1 && errno == EINTR)
        {
            continue;
        }

        break;
    }

    processId_ = -1;
}

void Process::reset() noexcept
{
    terminate();

    closeFd(stdinWrite_);
    closeFd(stdoutRead_);
}

bool Process::hasProcess() const noexcept
{
    return processId_ != -1;
}

#endif