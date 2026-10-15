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
    } // namespace

    void renderFenPanel(ChessGame& game, GuiGameController& controller)
    {
        ImGui::Begin("FEN");

        if (!bufferInitialized)
        {
            bufferInitialized = true;
            syncBufferFromGame(game);
        }

        ImGui::TextDisabled("Position FEN");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##fen", fenBuffer, kBufferSize);

        ImGui::Spacing();

        const bool apply = ImGui::Button("Apply");
        ImGui::SameLine();
        const bool refresh = ImGui::Button("Refresh");
        ImGui::SameLine();
        const bool copy = ImGui::Button("Copy");

        static std::string lastError;

        if (apply)
        {
            lastError.clear();
            const std::string candidate = fenBuffer;

            if (!looksLikeFen(candidate))
            {
                lastError = "This is not a valid FEN string.";
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
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
            ImGui::TextWrapped("%s", lastError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Apply replaces the current game and clears undo history.");

        ImGui::End();
    }

} // namespace Gui