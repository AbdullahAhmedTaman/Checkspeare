#include "UciEngine.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <windows.h>

struct UciEngine::Impl
{
    HANDLE processHandle = nullptr;
    HANDLE stdinWrite = nullptr;
    HANDLE stdoutRead = nullptr;

    std::jthread readerThread;

    std::mutex queueMutex;
    std::queue<std::string> outputQueue;

    std::atomic<bool> running = false;

    void start(const std::filesystem::path& path);
    void createPipes(HANDLE& childStdinRead, HANDLE& childStdoutWrite);
    void launchProcess(const std::filesystem::path& path, HANDLE childStdinRead, HANDLE childStdoutWrite);
    void startReaderThread();
    void writeLine(const std::string& line);

    static void closeHandle(HANDLE& handle);
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
    if (isRunning())
        stop();

    impl_->start(path);
}

void UciEngine::Impl::start(const std::filesystem::path& path)
{
    HANDLE childStdinRead = nullptr;
    HANDLE childStdoutWrite = nullptr;

    try
    {
        createPipes(childStdinRead, childStdoutWrite);

        launchProcess(path, childStdinRead, childStdoutWrite);

        // Parent no longer needs the child's copies.
        closeHandle(childStdinRead);
        closeHandle(childStdoutWrite);

        running = true;

        startReaderThread();
    }
    catch (...)
    {
        closeHandle(childStdinRead);
        closeHandle(childStdoutWrite);

        // If the process was already spawned (e.g. startReaderThread() is
        // what threw), it's still alive and orphaned unless we kill it here.
        if (processHandle != nullptr)
        {
            TerminateProcess(processHandle, 1);
            WaitForSingleObject(processHandle, INFINITE);
        }

        closeHandle(processHandle);
        closeHandle(stdinWrite);
        closeHandle(stdoutRead);

        running = false;

        throw;
    }
}

void UciEngine::Impl::createPipes(HANDLE& childStdinRead, HANDLE& childStdoutWrite)
{
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = nullptr;

    // Pipe: GUI -> Engine
    if (!CreatePipe(&childStdinRead, &stdinWrite, &securityAttributes, 0))
    {
        throw std::runtime_error("Failed to create stdin pipe.");
    }

    // Make the GUI's write end non-inheritable.
    if (!SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(stdinWrite);

        throw std::runtime_error("Failed to configure stdin pipe handle.");
    }

    // Pipe: Engine -> GUI
    if (!CreatePipe(&stdoutRead, &childStdoutWrite, &securityAttributes, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(stdinWrite);

        throw std::runtime_error("Failed to create stdout pipe.");
    }

    // Make the GUI's read end non-inheritable.
    if (!SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(stdinWrite);
        closeHandle(stdoutRead);
        closeHandle(childStdoutWrite);

        throw std::runtime_error("Failed to configure stdout pipe handle.");
    }
}

void UciEngine::Impl::launchProcess(const std::filesystem::path& path, HANDLE childStdinRead, HANDLE childStdoutWrite)
{
    // Configure child process I/O
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.hStdInput = childStdinRead;
    startupInfo.hStdOutput = childStdoutWrite;
    startupInfo.hStdError = childStdoutWrite;

    PROCESS_INFORMATION processInfo{};

    std::wstring executablePath = path.wstring();

    const BOOL success = CreateProcessW(executablePath.c_str(), // application name
                                        nullptr,                // command line
                                        nullptr,                // process security
                                        nullptr,                // thread security
                                        TRUE,                   // inherit handles
                                        CREATE_NO_WINDOW,       // don't create console window
                                        nullptr,                // environment
                                        nullptr,                // working directory
                                        &startupInfo, &processInfo);

    if (!success)
    {
        throw std::runtime_error("Failed to create UCI engine process.");
    }

    // We only need the process handle.
    closeHandle(processInfo.hThread);

    processHandle = processInfo.hProcess;
}

void UciEngine::Impl::startReaderThread()
{
    Impl* impl = this;

    // Captured by value (not read via impl->stdoutRead each iteration): if a
    // later start() reassigns the member while this thread is still alive,
    // it must keep reading from the pipe it was actually started with.
    HANDLE stdoutHandle = stdoutRead;

    readerThread = std::jthread(
        [impl, stdoutHandle](std::stop_token stop)
        {
            char buffer[4096];
            std::string pending;

            while (!stop.stop_requested())
            {
                DWORD bytesRead = 0;

                const BOOL success = ReadFile(stdoutHandle, buffer, sizeof(buffer), &bytesRead, nullptr);

                if (!success || bytesRead == 0)
                    break;

                pending.append(buffer, bytesRead);

                std::size_t lineStart = 0;

                while (true)
                {
                    const std::size_t newline = pending.find('\n', lineStart);

                    if (newline == std::string::npos)
                        break;

                    std::string line = pending.substr(lineStart, newline - lineStart);

                    // UCI engines normally use CRLF on Windows.
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();

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

            impl->running = false;
        });
}

void UciEngine::Impl::closeHandle(HANDLE& handle)
{
    if (handle != nullptr)
    {
        CloseHandle(handle);
        handle = nullptr;
    }
}

void UciEngine::Impl::writeLine(const std::string& line)
{
    const std::string data = line + '\n';

    DWORD bytesWritten = 0;

    const BOOL success = WriteFile(stdinWrite, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);

    if (!success || bytesWritten != data.size())
    {
        throw std::runtime_error("Failed to write to UCI engine.");
    }
}

bool UciEngine::isRunning() const
{
    // `running` is set true once the process is launched and cleared either
    // by stop(), by a failed start(), or by the reader thread itself when
    // ReadFile() hits EOF (i.e. the engine process has exited on its own).
    return impl_->running.load();
}

void UciEngine::stop()
{
    if (impl_->processHandle == nullptr)
        return;

    // Ask the engine to exit.
    if (impl_->stdinWrite != nullptr)
    {
        try
        {
            impl_->writeLine("quit");
        }
        catch (...)
        {
            // Ignore write failures during shutdown.
        }
    }

    // Wait for it to terminate.
    DWORD result = WaitForSingleObject(impl_->processHandle, 2000);

    // Force termination if necessary.
    if (result == WAIT_TIMEOUT)
    {
        TerminateProcess(impl_->processHandle, 1);

        // Wait until the process is definitely gone.
        WaitForSingleObject(impl_->processHandle, INFINITE);
    }

    if (impl_->readerThread.joinable())
    {
        impl_->readerThread.request_stop();
        impl_->readerThread.join();
    }

    // Clean up handles.
    Impl::closeHandle(impl_->stdinWrite);
    Impl::closeHandle(impl_->stdoutRead);
    Impl::closeHandle(impl_->processHandle);

    impl_->running = false;
}