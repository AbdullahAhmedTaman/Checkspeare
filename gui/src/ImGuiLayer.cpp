#include "ImGuiLayer.hpp"

#include "ChessClock.hpp"
#include "GameResult.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
        /// The single place where ActionResult is turned into human text.
        /// Additional consumers (sound, telemetry, logging) hook in here
        /// without touching GuiGameController.
        std::string formatActionResult(const ActionResult& result)
        {
            std::ostringstream oss;
            switch (result.kind)
            {
                case ActionResult::Kind::Ready:
                    return "Ready";
                case ActionResult::Kind::NotYourTurn:
                    return "Not your turn";
                case ActionResult::Kind::Dragging:
                    oss << "Dragging " << result.piece << " from " << result.from;
                    return oss.str();
                case ActionResult::Kind::MoveCancelled:
                    return "Move cancelled";
                case ActionResult::Kind::DroppedOutsideBoard:
                    return "Dropped outside board";
                case ActionResult::Kind::MovePlayed:
                    oss << "Played " << result.from << "-" << result.to;
                    return oss.str();
                case ActionResult::Kind::IllegalMove:
                    oss << "Illegal move " << result.from << "-" << result.to;
                    return oss.str();
                case ActionResult::Kind::PromotionPending:
                    return "Choose promotion piece on board";
            }
            return {};
        }

        /// Formats a millisecond count as mm:ss. Negative values clamp to 0.
        std::string formatClock(std::int64_t ms)
        {
            if (ms < 0)
                ms = 0;
            const std::int64_t totalSeconds = ms / 1000;
            const std::int64_t minutes = totalSeconds / 60;
            const std::int64_t seconds = totalSeconds % 60;

            char buf[16];
            std::snprintf(buf, sizeof(buf), "%02lld:%02lld", static_cast<long long>(minutes), static_cast<long long>(seconds));
            return buf;
        }

        /// Human-readable text for the game-over banner, or empty string
        /// when the game is still ongoing.
        std::string formatGameResult(const GameResult& r)
        {
            switch (r.kind)
            {
                case GameResultKind::Ongoing:
                    return "";
                case GameResultKind::Checkmate:
                    return (r.winner == WHITE) ? "Checkmate - White wins" : "Checkmate - Black wins";
                case GameResultKind::Stalemate:
                    return "Stalemate - Draw";
                case GameResultKind::FiftyMove:
                    return "Draw by fifty-move rule";
                case GameResultKind::Threefold:
                    return "Draw by threefold repetition";
                case GameResultKind::InsufficientMaterial:
                    return "Draw by insufficient material";
            }
            return "";
        }
    } // namespace

    void renderImGui(sf::RenderWindow& window, Config& conf, BoardGeometry& geo, const GuiGameController& controller,
                     const ChessGame& game, const ChessClock& clock)
    {
        // ---- Main panel --------------------------------------------------
        ImGui::Begin("Chess Engine Test");

        ImGui::Text("SFML + ImGui are working!");
        ImGui::Separator();

        const auto size = window.getSize();
        ImGui::Text("Window size: %u x %u", size.x, size.y);

        ImGui::SliderFloat("Square Size", &geo.squareSize, 20.0f, 150.0f);

        // SliderFloat2 needs a contiguous float[2]; copy in/out rather than
        // aliasing sf::Vector2f's storage.
        float origin[2] = {geo.boardOrigin.x, geo.boardOrigin.y};
        if (ImGui::SliderFloat2("Board Position", origin, 0.0f, 300.0f))
            geo.boardOrigin = {origin[0], origin[1]};

        ImGui::Checkbox("Flip Board", &geo.flipped);
        ImGui::Separator();

        ImGui::Text("Status:");
        const std::string status = formatActionResult(controller.result());
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s", status.c_str());

        ImGui::Separator();
        ImGui::Checkbox("Show ImGui Demo", &conf.showDemoWindow);

        ImGui::End();

        // ---- Clock panel -------------------------------------------------
        ImGui::Begin("Clock");

        const std::string white = formatClock(clock.remainingMs(WHITE));
        const std::string black = formatClock(clock.remainingMs(BLACK));

        // Highlight the side to move.
        const bool whiteToMove = (game.sideToMove() == WHITE);
        const ImVec4 activeColor = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
        const ImVec4 idleColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

        ImGui::TextColored(whiteToMove ? activeColor : idleColor, "White: %s", white.c_str());
        ImGui::TextColored(whiteToMove ? idleColor : activeColor, "Black: %s", black.c_str());

        if (clock.flagFallen(WHITE))
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "White's flag has fallen");
        if (clock.flagFallen(BLACK))
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Black's flag has fallen");

        ImGui::End();

        // ---- Game-over banner -------------------------------------------
        const GameResult result = game.result();
        const std::string resultText = formatGameResult(result);
        if (!resultText.empty())
        {
            ImGui::Begin("Game Over");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", resultText.c_str());
            ImGui::End();
        }

        // ---- ImGui demo -------------------------------------------------
        if (conf.showDemoWindow)
            ImGui::ShowDemoWindow(&conf.showDemoWindow);
    }

} // namespace Gui