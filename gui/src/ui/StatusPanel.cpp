#include "StatusPanel.hpp"

#include "../game/GameResult.hpp"

#include <imgui.h>

#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
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

        /// Counts of each piece type currently on the board, per color.
        /// Indexed [color][pieceType].
        struct PieceCounts
        {
            int per[2][6] = {};
        };

        /// Starting complement per color, in PieceType order:
        /// PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING.
        constexpr int kStarting[6] = {8, 2, 2, 2, 1, 1};

        /// Display order, heavy to light. Matches chess.com and Lichess.
        constexpr PieceType kDisplayOrder[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};

        PieceCounts countOnBoard(const Board& board)
        {
            PieceCounts counts;

            for (int i = 0; i < 64; ++i)
            {
                const Piece pc = board.pieceAt(static_cast<Square>(i));
                if (pc == PIECE_NONE)
                    continue;

                counts.per[color_of(pc)][type_of(pc)] += 1;
            }

            return counts;
        }

        /// Draws a single piece icon using AddImage with UVs.
        ///
        /// ImGui::Image stretches the whole texture into the target rectangle,
        /// which makes pieces with different amounts of transparent padding
        /// appear at different visual sizes. AddImage with explicit UVs lets
        /// us inset the sample region slightly, so every piece occupies the
        /// same fraction of its cell regardless of how the source PNG was
        /// framed.
        void drawPieceIcon(ImDrawList* drawList, const sf::Texture& texture, ImVec2 topLeft, float size)
        {
            constexpr float kUvInset = 0.04f;

            const ImVec2 bottomRight{topLeft.x + size, topLeft.y + size};

            const auto textureId = static_cast<ImTextureID>(texture.getNativeHandle());

            drawList->AddImage(textureId, topLeft, bottomRight, ImVec2(kUvInset, kUvInset),
                               ImVec2(1.0f - kUvInset, 1.0f - kUvInset));
        }

        /// Draws one row of captured-piece icons. `capturingSide` is the side
        /// whose captures are shown; the icons are the pieces of the *other*
        /// side that are missing from the board.
        void drawCapturedRow(const PieceCounts& onBoard, Color capturingSide, const Assets::TextureLibrary& assets,
                             float iconSize)
        {
            const Color capturedSide = (capturingSide == WHITE) ? BLACK : WHITE;

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Icons are laid out manually rather than through ImGui::SameLine
            // because AddImage bypasses ImGui's item system; SameLine and
            // NewLine only advance the layout cursor for ImGui items, not
            // for raw draw-list calls.
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            ImVec2 cursor = rowStart;

            for (PieceType type : kDisplayOrder)
            {
                const int missing = kStarting[type] - onBoard.per[capturedSide][type];

                for (int n = 0; n < missing; ++n)
                {
                    const std::string key = Assets::TextureLibrary::getPieceKey(make_piece(capturedSide, type));
                    const sf::Texture& texture = assets.getTexture(key);

                    drawPieceIcon(drawList, texture, cursor, iconSize);

                    cursor.x += iconSize + 2.0f;
                }
            }

            // Reserve the layout slot so the next widget starts on the next
            // line. Width is the full content width; height is one icon row.
            ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, iconSize));
        }
    } // namespace

    void renderStatusPanel(GuiGameController& controller, const ChessGame& game, bool canResign,
                           const Assets::TextureLibrary& assets)
    {
        ImGui::Begin("Game");

        const bool whiteToMove = (game.sideToMove() == WHITE);

        ImGui::TextDisabled("TURN");
        ImGui::SameLine();

        const ImU32 dotColor = whiteToMove ? IM_COL32(242, 241, 240, 255) : IM_COL32(30, 29, 28, 255);
        const ImU32 ringColor = whiteToMove ? IM_COL32(184, 183, 182, 255) : IM_COL32(128, 127, 126, 255);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 cur = ImGui::GetCursorScreenPos();
            const float lineH = ImGui::GetTextLineHeight();
            const ImVec2 center = ImVec2(cur.x + 5.0f, cur.y + lineH * 0.5f);
            dl->AddCircleFilled(center, 5.0f, dotColor);
            dl->AddCircle(center, 5.0f, ringColor, 0, 1.2f);
            ImGui::Dummy(ImVec2(16.0f, lineH));
        }

        ImGui::SameLine();
        ImGui::Text("%s", whiteToMove ? "White to move" : "Black to move");

        ImGui::Spacing();

        ImGui::TextDisabled("STATUS");
        ImGui::SameLine();
        const std::string status = formatActionResult(controller.result());
        ImGui::TextColored(ImVec4(0.94f, 0.72f, 0.30f, 1.0f), "%s", status.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Enabled only when the human side to move can give up. In human
        // vs engine this is the human's turn only; in engine vs engine it
        // is never enabled; in human vs human it tracks the clock. The
        // click resigns for the side to move, matching real chess.
        constexpr float kResignButtonWidth = 120.0f;
        const float availW = ImGui::GetContentRegionAvail().x;

        if (availW > kResignButtonWidth)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availW - kResignButtonWidth) * 0.5f);

        ImGui::BeginDisabled(!canResign);
        if (ImGui::Button("Resign", ImVec2(kResignButtonWidth, 0.0f)))
            controller.requestResign(game.sideToMove());
        ImGui::EndDisabled();

        ImGui::Spacing();

        // Captured pieces: top row is White's captures (Black pieces),
        // bottom row is Black's captures (White pieces).
        constexpr float kIconSize = 22.0f;
        const PieceCounts onBoard = countOnBoard(game.board());
        drawCapturedRow(onBoard, WHITE, assets, kIconSize);
        drawCapturedRow(onBoard, BLACK, assets, kIconSize);

        // Passive result banner. The modal is the active prompt; this is
        // a persistent indicator that survives after the modal is closed.
        const GameResult result = game.result();
        const std::string resultText = toString(result);
        if (!resultText.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const ImVec2 textSize = ImGui::CalcTextSize(resultText.c_str());
            const float cardH = textSize.y + 22.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.34f, 0.14f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.86f, 0.42f, 0.34f, 1.0f));
            ImGui::BeginChild("##result", ImVec2(0, cardH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

            // Center both axes within the child's content region. At the
            // top of a child, GetContentRegionAvail returns the full inner
            // rect; offsetting by half the slack places the text at the
            // visual center regardless of the theme's WindowPadding.
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float offsetX = (avail.x - textSize.x) * 0.5f;
            const float offsetY = (avail.y - textSize.y) * 0.5f;

            if (offsetX > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            if (offsetY > 0.0f)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

            ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.58f, 1.0f), "%s", resultText.c_str());

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        ImGui::End();
    }

} // namespace Gui