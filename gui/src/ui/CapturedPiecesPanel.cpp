#include "CapturedPiecesPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <string>

namespace Gui
{

    namespace
    {
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
        ///
        /// kUvInset is the fraction of the texture trimmed from each edge
        /// before sampling. 0.0 samples the full texture (identical to
        /// ImGui::Image); 0.05 trims 5% from each edge.
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
        void drawRow(const PieceCounts& onBoard, Color capturingSide, const Assets::TextureLibrary& assets, float iconSize)
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

                    // Debug aid. Uncomment if an icon appears to be the wrong
                    // color or piece; prints which key is being requested.
                    // std::fprintf(stderr, "[captured] side=%d type=%d key=%s\n",
                    //              static_cast<int>(capturedSide),
                    //              static_cast<int>(type),
                    //              key.c_str());

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

    void renderCapturedPiecesPanel(const Board& board, const Assets::TextureLibrary& assets)
    {
        ImGui::Begin("Captured");

        constexpr float kIconSize = 22.0f;

        const PieceCounts onBoard = countOnBoard(board);

        // Top row: White's captures, which are Black pieces.
        drawRow(onBoard, WHITE, assets, kIconSize);

        // Bottom row: Black's captures, which are White pieces.
        drawRow(onBoard, BLACK, assets, kIconSize);

        ImGui::End();
    }

} // namespace Gui