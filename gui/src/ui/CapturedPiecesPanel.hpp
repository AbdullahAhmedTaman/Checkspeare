#pragma once

#include "../assets/TextureLibrary.hpp"
#include "../game/ChessGame.hpp"

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace Gui
{

    struct CapturedGroup
    {
        PieceType type = PAWN;
        int count = 0;
    };

    std::vector<CapturedGroup> capturedBy(const ChessGame& game, Color capturingSide);
    float drawCapturedIcon(ImDrawList* drawList, const sf::Texture& texture, ImVec2 topLeft, float size, int count);
    /// Renders the "Captured" panel: two rows, one per side, showing the
    /// opponent pieces that side has captured so far. The panel reads the
    /// board state directly; it does not consume the move history, so undo
    /// and jump-to-ply are handled for free.
    void renderCapturedPiecesPanel(const ChessGame& game, const Assets::TextureLibrary& assets);
} // namespace Gui