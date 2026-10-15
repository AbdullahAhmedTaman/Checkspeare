#include "CapturedPiecesPanel.hpp"

#include "../game/Board.hpp"

#include <imgui.h>

#include <cstddef>
#include <string>

namespace Gui
{

    namespace
    {
        /// Display order, heavy to light. Matches chess.com and Lichess.
        constexpr PieceType kDisplayOrder[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};

        bool isCapturable(PieceType pt)
        {
            return pt != KING && pt != NONE;
        }

        /// Walks the game forward from the start position, replaying every ply
        /// up to the cursor against a scratch board, and records every capture.
        ///
        /// Replaying rather than reading the live board is what lets a promoted
        /// piece be reported as the promoted type: at the moment of capture,
        /// the scratch board still reflects the promoted piece sitting on the
        /// square, so we read its type directly.
        void accumulateCaptures(const ChessGame& game, int out[2][6])
        {
            // Replay from the position that started this game's history,
            // not necessarily the standard starting position.
            Board scratch;
            scratch.loadFEN(game.rootFEN());

            const auto& history = game.history();
            const std::size_t cursor = std::min(game.cursor(), history.size());

            for (std::size_t i = 0; i < cursor; ++i)
            {
                const Move m = history[i].move;

                if (m.is_capture())
                {
                    if (m.is_ep())
                    {
                        const Color mover = scratch.sideToMove();
                        out[mover][PAWN] += 1;
                    }
                    else
                    {
                        const Piece victimPiece = scratch.pieceAt(m.to());

                        if (victimPiece != PIECE_NONE)
                        {
                            const Color mover = color_of(scratch.pieceAt(m.from()));

                            const PieceType victimType = type_of(victimPiece);

                            if (isCapturable(victimType))
                                out[mover][victimType] += 1;
                        }
                    }
                }

                scratch.applyMove(m);
            }
        }
    } // namespace

    std::vector<CapturedGroup> capturedBy(const ChessGame& game, Color capturingSide)
    {
        int counts[2][6] = {};
        accumulateCaptures(game, counts);

        std::vector<CapturedGroup> groups;
        groups.reserve(5);

        for (PieceType type : kDisplayOrder)
        {
            const int n = counts[capturingSide][type];
            if (n > 0)
                groups.push_back({type, n});
        }

        return groups;
    }

    float drawCapturedIcon(ImDrawList* drawList, const sf::Texture& texture, ImVec2 topLeft, float size, int count)
    {
        constexpr float kUvInset = 0.04f;

        const ImVec2 bottomRight{topLeft.x + size, topLeft.y + size};
        const auto textureId = static_cast<ImTextureID>(texture.getNativeHandle());

        drawList->AddImage(textureId, topLeft, bottomRight, ImVec2(kUvInset, kUvInset), ImVec2(1.0f - kUvInset, 1.0f - kUvInset));

        if (count <= 1)
            return size;

        // The count is drawn to the right of the icon, vertically centered.
        // Accent amber so it reads as annotation rather than as content, and
        // pairs with the same color used in the clock and eval bar.
        const std::string label = "x" + std::to_string(count);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());

        const float badgeX = topLeft.x + size + 2.0f;
        const float badgeY = topLeft.y + (size - textSize.y) * 0.5f;

        drawList->AddText(ImVec2(badgeX, badgeY), IM_COL32(232, 200, 122, 255), label.c_str());

        return size + 2.0f + textSize.x;
    }

    void renderCapturedPiecesPanel(const ChessGame& game, const Assets::TextureLibrary& assets)
    {
        ImGui::Begin("Captured");

        constexpr float kIconSize = 22.0f;
        constexpr float kGroupGap = 6.0f;

        const auto drawRow = [&](Color capturingSide)
        {
            const Color capturedSide = (capturingSide == WHITE) ? BLACK : WHITE;
            const std::vector<CapturedGroup> groups = capturedBy(game, capturingSide);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();

            for (const CapturedGroup& g : groups)
            {
                const std::string key = Assets::TextureLibrary::getPieceKey(make_piece(capturedSide, g.type));
                const sf::Texture& texture = assets.getTexture(key);

                cursor.x += drawCapturedIcon(drawList, texture, cursor, kIconSize, g.count) + kGroupGap;
            }

            ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, kIconSize));
        };

        // Top row: White's captures (i.e. Black pieces).
        drawRow(WHITE);

        // Bottom row: Black's captures (i.e. White pieces).
        drawRow(BLACK);

        ImGui::End();
    }

} // namespace Gui