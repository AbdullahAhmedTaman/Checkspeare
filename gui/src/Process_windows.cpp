#ifdef _WIN32

#include "Process.hpp"

#include <limits>
#include <stdexcept>
#include <system_error>

namespace
{
    void closeHandle(HANDLE& handle) noexcept
    {
        if (handle != nullptr)
        {
            CloseHandle(handle);
            handle = nullptr;
        }
    }

    [[noreturn]]
    void throwLastError(const char* message)
    {
        const DWORD error = GetLastError();

        throw std::system_error(static_cast<int>(error), std::system_category(), message);
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

    HANDLE childStdinRead = nullptr;
    HANDLE childStdoutWrite = nullptr;

    HANDLE parentStdinWrite = nullptr;
    HANDLE parentStdoutRead = nullptr;

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);

    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = nullptr;

    // Pipe: GUI -> Engine
    if (!CreatePipe(&childStdinRead, &parentStdinWrite, &securityAttributes, 0))
    {
        throwLastError("Failed to create stdin pipe.");
    }

    // Make the GUI's write end non-inheritable.
    if (!SetHandleInformation(parentStdinWrite, HANDLE_FLAG_INHERIT, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(parentStdinWrite);

        throwLastError("Failed to configure stdin pipe handle.");
    }

    // Pipe: Engine -> GUI
    if (!CreatePipe(&parentStdoutRead, &childStdoutWrite, &securityAttributes, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(parentStdinWrite);

        throwLastError("Failed to create stdout pipe.");
    }

    // Make the GUI's read end non-inheritable.
    if (!SetHandleInformation(parentStdoutRead, HANDLE_FLAG_INHERIT, 0))
    {
        closeHandle(childStdinRead);
        closeHandle(parentStdinWrite);
        closeHandle(parentStdoutRead);
        closeHandle(childStdoutWrite);

        throwLastError("Failed to configure stdout pipe handle.");
    }

    // Configure child process I/O
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;

    startupInfo.hStdInput = childStdinRead;
    startupInfo.hStdOutput = childStdoutWrite;
    startupInfo.hStdError = childStdoutWrite;

    PROCESS_INFORMATION processInfo{};

    const std::wstring executablePath = executable.wstring();

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
        closeHandle(childStdinRead);
        closeHandle(parentStdinWrite);
        closeHandle(parentStdoutRead);
        closeHandle(childStdoutWrite);

        throwLastError("Failed to create UCI engine process.");
    }

    // Parent no longer needs the child's copies.
    closeHandle(childStdinRead);
    closeHandle(childStdoutWrite);

    // We only need the process handle.
    closeHandle(processInfo.hThread);

    processHandle_ = processInfo.hProcess;
    stdinWrite_ = parentStdinWrite;
    stdoutRead_ = parentStdoutRead;
}

bool Process::write(std::string_view data)
{
    if (stdinWrite_ == nullptr)
        return false;

    std::size_t offset = 0;

    while (offset < data.size())
    {
        const std::size_t remaining = data.size() - offset;

        const std::size_t maxDwordSize = static_cast<std::size_t>(std::numeric_limits<DWORD>::max());

        const DWORD bytesToWrite = static_cast<DWORD>(remaining < maxDwordSize ? remaining : maxDwordSize);

        DWORD bytesWritten = 0;

        const BOOL success = WriteFile(stdinWrite_, data.data() + offset, bytesToWrite, &bytesWritten, nullptr);

        if (!success)
            return false;

        if (bytesWritten == 0)
            return false;

        offset += bytesWritten;
    }

    return true;
}

std::ptrdiff_t Process::read(char* buffer, std::size_t size)
{
    if (stdoutRead_ == nullptr || buffer == nullptr)
        return -1;

    if (size == 0)
        return 0;

    const std::size_t maxDwordSize = static_cast<std::size_t>(std::numeric_limits<DWORD>::max());

    const DWORD bytesToRead = static_cast<DWORD>(size < maxDwordSize ? size : maxDwordSize);

    DWORD bytesRead = 0;

    const BOOL success = ReadFile(stdoutRead_, buffer, bytesToRead, &bytesRead, nullptr);

    if (!success)
    {
        const DWORD error = GetLastError();

        if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF)
        {
            return 0;
        }

        return -1;
    }

    if (bytesRead == 0)
        return 0;

    return static_cast<std::ptrdiff_t>(bytesRead);
}

bool Process::waitForExit(std::chrono::milliseconds timeout) noexcept
{
    if (processHandle_ == nullptr)
        return true;

    const auto count = timeout.count();

    DWORD timeoutMs = 0;

    if (count <= 0)
    {
        timeoutMs = 0;
    }
    else if (static_cast<unsigned long long>(count) > static_cast<unsigned long long>(std::numeric_limits<DWORD>::max()))
    {
        timeoutMs = INFINITE;
    }
    else
    {
        timeoutMs = static_cast<DWORD>(count);
    }

    const DWORD result = WaitForSingleObject(processHandle_, timeoutMs);

    return result == WAIT_OBJECT_0;
}

void Process::terminate() noexcept
{
    if (processHandle_ == nullptr)
        return;

    TerminateProcess(processHandle_, 1);

    WaitForSingleObject(processHandle_, INFINITE);
}

void Process::reset() noexcept
{
    terminate();

    closeHandle(stdinWrite_);
    closeHandle(stdoutRead_);
    closeHandle(processHandle_);
}

bool Process::hasProcess() const noexcept
{
    return processHandle_ != nullptr;
}

#endif