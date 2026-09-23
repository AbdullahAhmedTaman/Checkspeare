#include "EnginePlayer.hpp"

// EnginePlayer.cpp

namespace
{
    /// Decodes a UCI square like "e2" into an engine Square.
    /// Rejects anything not in [a..h][1..8]; case-sensitive per UCI spec.
    std::optional<Square> parseSquare(std::string_view fileRank)
    {
        if (fileRank.size() != 2)
            return std::nullopt;

        const char fileChar = fileRank[0];
        const char rankChar = fileRank[1];

        if (fileChar < 'a' || fileChar > 'h')
            return std::nullopt;
        if (rankChar < '1' || rankChar > '8')
            return std::nullopt;

        const int file = fileChar - 'a';
        const int rank = rankChar - '1';

        return static_cast<Square>(rank * 8 + file);
    }

    /// Decodes a promotion suffix ('q', 'r', 'b', 'n').
    /// Any other character returns nullopt.
    std::optional<PieceType> parsePromotionChar(char c)
    {
        switch (c)
        {
            case 'q':
                return QUEEN;
            case 'r':
                return ROOK;
            case 'b':
                return BISHOP;
            case 'n':
                return KNIGHT;
            default:
                return std::nullopt;
        }
    }

    /// Extracts the moves already played (up to cursor) as a plain vector.
    /// Feeds UciEngine::setPosition's `moves` argument.
    std::vector<Move> playedMoves(const ChessGame& game)
    {
        std::vector<Move> moves;
        moves.reserve(game.cursor());

        const auto& hist = game.history();
        for (std::size_t i = 0; i < game.cursor(); ++i)
            moves.push_back(hist[i].move);

        return moves;
    }

    /// Builds `go` limits from the current clock state.
    /// If the clock is unconfigured (both sides at 0), falls back to a fixed
    /// movetime so the engine cannot search forever.
    UciSearchLimits limitsFrom(const ChessClock& clock)
    {
        UciSearchLimits limits;

        const std::int64_t w = clock.remainingMs(WHITE);
        const std::int64_t b = clock.remainingMs(BLACK);

        if (w > 0 && b > 0)
        {
            limits.wtimeMs = w;
            limits.btimeMs = b;
            // Increments are not yet exposed by ChessClock; omitted for now.
        }
        else
        {
            limits.movetimeMs = 1000;
        }

        return limits;
    }
} // namespace

std::optional<Move> parseUciMove(const Board& board, std::string_view text)
{
    if (text.size() != 4 && text.size() != 5)
        return std::nullopt;

    const auto from = parseSquare(text.substr(0, 2));
    const auto to = parseSquare(text.substr(2, 2));

    if (!from || !to)
        return std::nullopt;

    for (const Move& m : board.legalMovesFrom(*from))
    {
        if (m.to() != *to)
            continue;

        if (m.is_promotion())
        {
            // A promotion move must carry an explicit piece letter.
            if (text.size() != 5)
                continue;

            const auto promo = parsePromotionChar(text[4]);
            if (!promo || promotion_type(m) != *promo)
                continue;

            return m;
        }

        // A non-promotion move must not carry a suffix.
        if (text.size() == 5)
            continue;

        return m;
    }

    return std::nullopt;
}

EnginePlayerAction decideEngineAction(const EnginePlayerInputs& in)
{
    if (!in.thinking)
    {
        // Only start a search when the side to move is engine-controlled
        // and the engine is alive and past its uci/isready handshake.
        if (!in.sideControlled)
            return EnginePlayerAction::Idle;
        if (!in.engineAlive || !in.engineReady)
            return EnginePlayerAction::Idle;
        return EnginePlayerAction::StartSearch;
    }

    // Already thinking. Priority order matters:
    //   1. The game moved on -> the in-flight result is stale; abort.
    //   2. A bestmove is available -> use it, unless (1) already won.
    //   3. The engine died -> abort; there will be no bestmove.
    //   4. Otherwise keep waiting.
    if (in.gameCursor != in.thinkStartPly)
        return EnginePlayerAction::AbortSearch;

    if (in.bestMoveArrived)
        return EnginePlayerAction::FinishSearch;

    if (!in.engineAlive)
        return EnginePlayerAction::AbortSearch;

    return EnginePlayerAction::Idle;
}

EnginePlayer::EnginePlayer(UciEngine& engine)
    : engine_(engine)
{
}

void EnginePlayer::setSideControlled(Color side, bool controlled)
{
    controlled_[static_cast<int>(side)] = controlled;
}

bool EnginePlayer::isSideControlled(Color side) const
{
    return controlled_[static_cast<int>(side)];
}

void EnginePlayer::abort()
{
    if (!thinking_)
        return;

    engine_.stopSearch();
    thinking_ = false;
}

std::optional<Move> EnginePlayer::tick(ChessGame& game, ChessClock& clock)
{
    // poll() must run every frame, not just while thinking. It advances the
    // UCI handshake (uciok -> isready -> readyok) and drains the output queue.
    UciPollResult poll = engine_.poll();

    EnginePlayerInputs in;
    in.thinking = thinking_;
    in.thinkStartPly = thinkStartPly_;
    in.gameCursor = game.cursor();
    in.sideToMove = game.sideToMove();
    in.sideControlled = controlled_[static_cast<int>(game.sideToMove())];
    in.engineReady = engine_.isReady();
    in.engineAlive = engine_.isRunning();
    in.bestMoveArrived = poll.bestMove.has_value();

    switch (decideEngineAction(in))
    {
        case EnginePlayerAction::Idle:
            return std::nullopt;

        case EnginePlayerAction::StartSearch:
            {
                // Empty FEN -> UciEngine emits "position startpos moves ...",
                // which is correct because ChessGame always starts from the
                // standard initial position.
                engine_.setPosition("", playedMoves(game));
                engine_.go(limitsFrom(clock));

                thinking_ = true;
                thinkStartPly_ = game.cursor();
                return std::nullopt;
            }

        case EnginePlayerAction::AbortSearch:
            engine_.stopSearch();
            thinking_ = false;
            return std::nullopt;
        case EnginePlayerAction::FinishSearch:
            {
                thinking_ = false;

                if (!poll.bestMove)
                    return std::nullopt;

                const auto m = parseUciMove(game.board(), *poll.bestMove);

                if (!m)
                    return std::nullopt;

                const PieceType promo = m->is_promotion() ? promotion_type(*m) : QUEEN;
                auto played = game.play(m->from(), m->to(), promo);

                return played;
            }
    }

    return std::nullopt;
}