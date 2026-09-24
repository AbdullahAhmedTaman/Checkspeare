#include "BoardPanel.hpp"

#include "BoardRenderer.hpp"

#include <imgui-SFML.h>
#include <imgui.h>

#include <algorithm>

namespace Gui
{

    void BoardPanel::render(BoardGeometry& geo, const BoardTheme& theme, const Board& board, const Assets::TextureLibrary& assets,
                            const sf::Font& coordinateFont, const DragState& drag, const PendingPromotion& promo,
                            const BoardAnnotations& annotations)
    {
        // The board texture is rendered at this multiple of the display size,
        // then downsampled by ImGui's texture sampler. 2x is a big improvement
        // over 1x; 3x is marginally better. The board is not fill-heavy, so
        // even 3x is cheap.
        constexpr unsigned kSupersample = 2;

        ImGui::Begin("Board");

        const ImVec2 avail = ImGui::GetContentRegionAvail();

        // Display size — what the user sees and clicks on.
        float displaySide = std::min(avail.x, avail.y);
        if (displaySide < 64.0f)
            displaySide = 64.0f;

        const unsigned displaySize = static_cast<unsigned>(displaySide);

        // Texture size — what we draw into. 2x in each dimension, so 4x the
        // pixels, 4x the fill-rate cost of the board pass.
        const unsigned texSize = displaySize * kSupersample;

        if (texture_.getSize().x != texSize || texture_.getSize().y != texSize)
        {
            if (!texture_.resize({texSize, texSize}))
            {
                ImGui::TextUnformatted("Failed to create board render target.");
                ImGui::End();
                return;
            }
        }

        // geo describes the *texture*, not the display. The controller and the
        // renderer both operate in texture space; only the ImGui::Image call
        // (and the pointer rebasing below) needs to know about the difference.
        geo.boardOrigin = {0.0f, 0.0f};
        geo.squareSize = static_cast<float>(texSize) / 8.0f;

        const float slack = avail.x - static_cast<float>(displaySize);
        if (slack > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack * 0.5f);

        origin_ = {ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y};
        size_ = {static_cast<float>(displaySize), static_cast<float>(displaySize)};

        // Rebase pointer from window space into *texture* space. The factor of
        // kSupersample is what keeps the drag sprite aligned with the cursor
        // even though the texture is larger than the widget.
        const ImVec2 mouseScreen = ImGui::GetMousePos();
        const sf::Vector2f mouseTex{(mouseScreen.x - origin_.x) * kSupersample, (mouseScreen.y - origin_.y) * kSupersample};

        texture_.clear(sf::Color::White);
        Gui::renderBoard(texture_, geo, theme, board, assets, coordinateFont, drag, mouseTex, promo, annotations);
        texture_.display();

        const auto textureId = static_cast<ImTextureID>(texture_.getTexture().getNativeHandle());

        // Display at display size — ImGui's sampler downsamples the 2x texture.
        ImGui::Image(textureId, ImVec2(static_cast<float>(displaySize), static_cast<float>(displaySize)), ImVec2(0.0f, 1.0f),
                     ImVec2(1.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::End();
    }

    bool BoardPanel::contains(sf::Vector2f windowPos) const
    {
        return windowPos.x >= origin_.x && windowPos.x < origin_.x + size_.x && windowPos.y >= origin_.y &&
               windowPos.y < origin_.y + size_.y;
    }

    sf::Vector2f BoardPanel::toBoardSpace(sf::Vector2f windowPos) const
    {
        // geo describes the texture; the widget on screen is smaller by the
        // supersample factor. Scale window-space deltas up so the controller
        // sees the same units geo does.
        const float scale = (size_.x > 0.0f) ? static_cast<float>(texture_.getSize().x) / size_.x : 1.0f;
        return {(windowPos.x - origin_.x) * scale, (windowPos.y - origin_.y) * scale};
    }

} // namespace Gui