#pragma once

#include "types.h"

#include <cstdint>
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
    int scoreCp = 0;
    int scoreMate = 0;
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
    /// @note Not yet implemented.
    UciEngine();

    /// @note Not yet implemented.
    ~UciEngine();

    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    /// Spawns the engine at `path`. Throws std::runtime_error on failure.
    /// @note Not yet implemented.
    void start(const std::string& path);

    /// Sends `quit` and joins the reader thread. Idempotent.
    /// @note Not yet implemented.
    void stop();

    /// @note Not yet implemented.
    bool isRunning() const;

    /// @note Not yet implemented.
    void newGame();

    /// @note Not yet implemented.
    void setPosition(const std::string& fen, const std::vector<Move>& moves);

    /// @note Not yet implemented.
    void go(const UciSearchLimits& limits);

    /// @note Not yet implemented.
    void stopSearch();

    /// Drains pending output. Never blocks.
    /// @note Not yet implemented.
    UciPollResult poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};