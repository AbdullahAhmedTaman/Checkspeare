#include "EnginePlayer.hpp"

namespace
{
    /// Decodes a UCI square like "e2" into an engine Square.
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

    std::vector<Move> playedMoves(const ChessGame& game)
    {
        std::vector<Move> moves;
        moves.reserve(game.cursor());

        const auto& hist = game.history();
        for (std::size_t i = 0; i < game.cursor(); ++i)
            moves.push_back(hist[i].move);

        return moves;
    }

    UciSearchLimits limitsFrom(const ChessClock& clock)
    {
        UciSearchLimits limits;

        const std::int64_t w = clock.remainingMs(WHITE);
        const std::int64_t b = clock.remainingMs(BLACK);

        if (w > 0 && b > 0)
        {
            limits.wtimeMs = w;
            limits.btimeMs = b;
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
            if (text.size() != 5)
                continue;

            const auto promo = parsePromotionChar(text[4]);
            if (!promo || promotion_type(m) != *promo)
                continue;

            return m;
        }

        if (text.size() == 5)
            continue;

        return m;
    }

    return std::nullopt;
}

EnginePlayerAction decideEngineAction(const EnginePlayerInputs& in)
{
    // Already thinking. Priority order:
    //   1. Game over      -> abort any in-flight search.
    //   2. Game moved on  -> abort; result is stale.
    //   3. bestmove ready -> finish and play it.
    //   4. Engine died    -> abort; there will be no bestmove.
    //   5. Otherwise idle.
    if (in.thinking)
    {
        if (in.gameOver)
            return EnginePlayerAction::AbortSearch;

        if (in.gameCursor != in.thinkStartPly)
            return EnginePlayerAction::AbortSearch;

        if (in.bestMoveArrived)
            return EnginePlayerAction::FinishSearch;

        if (!in.engineAlive)
            return EnginePlayerAction::AbortSearch;

        return EnginePlayerAction::Idle;
    }

    // Not thinking. Do not start a search if the game is over.
    if (in.gameOver)
        return EnginePlayerAction::Idle;

    if (!in.sideControlled)
        return EnginePlayerAction::Idle;
    if (!in.engineAlive || !in.engineReady)
        return EnginePlayerAction::Idle;

    // Only auto-play at the live tip. If the cursor is behind the tip the
    // user is reviewing history (undo / redo / gotoPly), and starting a
    // search would be destructive: ChessGame::play truncates moves_ at the
    // cursor, wiping the redo line being browsed. Returning to the tip
    // (redo, gotoEnd, or playing a move from the review position) will
    // re-enable auto-play naturally on the next tick.
    if (in.gameCursor != in.historySize)
        return EnginePlayerAction::Idle;

    return EnginePlayerAction::StartSearch;
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
    UciPollResult poll = engine_.poll();
    if (!poll.infos.empty())
        lastInfo_ = poll.infos.back();

    EnginePlayerInputs in;
    in.thinking = thinking_;
    in.thinkStartPly = thinkStartPly_;
    in.gameCursor = game.cursor();
    in.historySize = game.history().size();
    in.sideToMove = game.sideToMove();
    in.sideControlled = controlled_[static_cast<int>(game.sideToMove())];

    // gameOver includes both terminal positions (mate, stalemate, draws)
    // and clock flag fall. Game and clock are separate sources of truth;
    // this is the one place that combines them.
    in.gameOver = (game.result().kind != GameResultKind::Ongoing) || clock.flagFallen(WHITE) || clock.flagFallen(BLACK);

    in.engineReady = engine_.isReady();
    in.engineAlive = engine_.isRunning();
    in.bestMoveArrived = poll.bestMove.has_value();

    switch (decideEngineAction(in))
    {
        case EnginePlayerAction::Idle:
            return std::nullopt;

        case EnginePlayerAction::StartSearch:
            {
                engine_.setPosition("", playedMoves(game));
                engine_.go(limitsFrom(clock));

                thinking_ = true;
                thinkStartPly_ = game.cursor();
                lastInfo_.reset();
                lastInfoSide_ = game.sideToMove();
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