#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string_view>

#ifdef _WIN32

#include <windows.h>

#else

#include <sys/types.h>

#endif

class Process
{
public:
    Process() = default;
    ~Process() noexcept;

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    Process(Process&&) = delete;
    Process& operator=(Process&&) = delete;

    void start(const std::filesystem::path& executable);

    [[nodiscard]]
    bool write(std::string_view data);

    [[nodiscard]]
    std::ptrdiff_t read(char* buffer, std::size_t size);

    [[nodiscard]]
    bool waitForExit(std::chrono::milliseconds timeout) noexcept;

    void terminate() noexcept;

    void reset() noexcept;

    [[nodiscard]]
    bool hasProcess() const noexcept;

private:
#ifdef _WIN32

    HANDLE processHandle_ = nullptr;
    HANDLE stdinWrite_ = nullptr;
    HANDLE stdoutRead_ = nullptr;

#else

    pid_t processId_ = -1;
    int stdinWrite_ = -1;
    int stdoutRead_ = -1;

#endif
};