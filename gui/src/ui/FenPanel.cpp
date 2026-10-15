#include "FenPanel.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>

namespace Gui
{

    namespace
    {
        /// Text buffer for the FEN field. 128 bytes comfortably covers the
        /// longest legal FEN; longer input is truncated by the widget.
        constexpr std::size_t kBufferSize = 128;

        char fenBuffer[kBufferSize] = {};
        bool bufferInitialized = false;

        void syncBufferFromGame(const ChessGame& game)
        {
            const std::string fen = game.board().position().to_fen();
            std::strncpy(fenBuffer, fen.c_str(), kBufferSize - 1);
            fenBuffer[kBufferSize - 1] = '\0';
        }

        bool looksLikeFen(const std::string& s)
        {
            // Must be non-empty.
            if (s.empty())
                return false;

            // Split on whitespace.
            std::istringstream iss(s);
            std::string fields[6];
            int count = 0;
            while (count < 6 && iss >> fields[count])
                ++count;

            // A legal FEN has 4 or 6 fields: the last two (halfmove clock
            // and fullmove number) may be omitted by some producers.
            if (count != 4 && count != 6)
                return false;

            // Field 0: piece placement. Exactly 8 ranks separated by '/',
            // each rank summing to 8 files.
            const std::string& placement = fields[0];
            int rankSum = 0;
            int rankCount = 1;
            for (char c : placement)
            {
                if (c == '/')
                {
                    if (rankSum != 8)
                        return false;
                    rankSum = 0;
                    ++rankCount;
                    continue;
                }
                if (c >= '1' && c <= '8')
                {
                    rankSum += c - '0';
                    continue;
                }
                if (std::isalpha(static_cast<unsigned char>(c)))
                {
                    ++rankSum;
                    continue;
                }
                return false;
            }
            if (rankCount != 8 || rankSum != 8)
                return false;

            // Field 1: side to move.
            if (fields[1] != "w" && fields[1] != "b")
                return false;

            // Field 2: castling rights. '-' or a subset of KQkq.
            const std::string& castling = fields[2];
            if (castling != "-")
            {
                for (char c : castling)
                    if (c != 'K' && c != 'Q' && c != 'k' && c != 'q')
                        return false;
            }

            // Field 3: en passant target. '-' or a square like "e3".
            const std::string& ep = fields[3];
            if (ep != "-")
            {
                if (ep.size() != 2)
                    return false;
                if (ep[0] < 'a' || ep[0] > 'h')
                    return false;
                if (ep[1] < '1' || ep[1] > '8')
                    return false;
            }

            // Fields 4 and 5, if present, must be decimal numbers.
            for (int i = 4; i < count; ++i)
            {
                if (fields[i].empty())
                    return false;
                for (char c : fields[i])
                    if (!std::isdigit(static_cast<unsigned char>(c)))
                        return false;
            }

            return true;
        }

        /// Width of a button sized to its label plus frame padding.
        float buttonWidth(const char* label)
        {
            return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }
    } // namespace

    void renderFenPanel(ChessGame& game, GuiGameController& controller)
    {
        ImGui::Begin("FEN");

        // Tighten the layout so the whole panel fits on a single row.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));

        if (!bufferInitialized)
        {
            bufferInitialized = true;
            syncBufferFromGame(game);
        }

        const float applyW = buttonWidth("Apply");
        const float refreshW = buttonWidth("Refresh");
        const float copyW = buttonWidth("Copy");
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        // [label] [input] [Apply] [Refresh] [Copy]
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("FEN");
        ImGui::SameLine();

        const float inputWidth = ImGui::GetContentRegionAvail().x - (applyW + refreshW + copyW) - (spacing * 4.0f);

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputText("##fen", fenBuffer, kBufferSize);

        ImGui::SameLine();
        const bool apply = ImGui::Button("Apply", ImVec2(applyW, 0.0f));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Apply replaces the current game and clears undo history.");

        ImGui::SameLine();
        const bool refresh = ImGui::Button("Refresh", ImVec2(refreshW, 0.0f));

        ImGui::SameLine();
        const bool copy = ImGui::Button("Copy", ImVec2(copyW, 0.0f));

        static std::string lastError;

        if (apply)
        {
            lastError.clear();
            const std::string candidate = fenBuffer;

            if (!looksLikeFen(candidate))
            {
                lastError = "Not a valid FEN.";
            }
            else
            {
                try
                {
                    game.loadFEN(candidate);
                    controller.clearArrows();
                    controller.cancelSelection();
                }
                catch (const std::exception& e)
                {
                    lastError = e.what();
                }
            }
        }

        if (refresh)
        {
            syncBufferFromGame(game);
            lastError.clear();
        }

        if (copy)
        {
            ImGui::SetClipboardText(fenBuffer);
        }

        if (!lastError.empty())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
            ImGui::TextUnformatted(lastError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar(2);

        ImGui::End();
    }

} // namespace Gui