#pragma once

#include "types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// One `info ...` line from the engine, reduced to the fields the UI uses.
/// `raw` is the original line, kept for logging.
struct UciInfo
{
    int depth = 0;
    int seldepth = 0;
    std::int64_t nodes = 0;
    std::int64_t nps = 0;
    std::int64_t timeMs = 0;
    std::optional<int> scoreCp;
    std::optional<int> scoreMate;
    std::vector<Move> pv;
    std::string raw;
};

struct UciPollResult
{
    std::vector<UciInfo> infos;
    std::optional<Move> bestMove;
};

/// Limits for a `go` command. Unset optional fields are omitted from the
/// wire message.
struct UciSearchLimits
{
    std::optional<std::int64_t> wtimeMs;
    std::optional<std::int64_t> btimeMs;
    std::optional<std::int64_t> wincMs;
    std::optional<std::int64_t> bincMs;
    std::optional<std::int64_t> movetimeMs;
    std::optional<int> depth;
    bool infinite = false;
};

/// Manages a UCI engine subprocess and speaks the protocol.
///
/// A single reader thread drains the child's stdout into an internal queue;
/// poll() drains that queue on the caller's thread. Writes happen only on
/// the caller's thread, so no write-side synchronization is needed.
///
/// Lifetime: start() spawns; the destructor stops. All methods are safe
/// no-ops if the engine was never started.
class UciEngine
{
public:
    UciEngine();

    ~UciEngine();

    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    UciEngine(UciEngine&&) = delete;
    UciEngine& operator=(UciEngine&&) = delete;

    /// Spawns the engine at `path`. Throws std::runtime_error on failure.
    void start(const std::filesystem::path& path);

    /// Stops the engine and joins the reader thread. Idempotent.
    void stop();

    /// True from a successful start() until the engine process exits
    /// (detected by the reader thread hitting EOF) or stop() is called.
    bool isRunning() const;

    void newGame();

    void setPosition(const std::string& fen, const std::vector<Move>& moves);

    /// Validation against contradictory limits argument is not yet implemented.
    void go(const UciSearchLimits& limits);

    void stopSearch();

    /// Drains pending output. Never blocks.
    /// @note Not yet implemented.
    UciPollResult poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};