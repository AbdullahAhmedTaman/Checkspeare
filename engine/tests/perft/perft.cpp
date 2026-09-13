#include "perft.h"
#include "../../include/movegen.h"
#include "../../include/types.h"
#include <chrono>
#include <iostream>

namespace
{
    /**
     * @brief Formats a Move object into standard UCI string representation (e.g., "e2e4", "a7a8q").
     *
     * @param move The Move to format.
     */
    void print_uci_move(const Move& move)
    {
        Square from_sq = move.from();
        Square to_sq = move.to();

        char from_file = static_cast<char>('a' + (from_sq % 8));
        char from_rank = static_cast<char>('1' + (from_sq / 8));
        char to_file = static_cast<char>('a' + (to_sq % 8));
        char to_rank = static_cast<char>('1' + (to_sq / 8));

        std::cout << from_file << from_rank << to_file << to_rank;

        if (move.is_promotion())
        {
            PieceType promo = promotion_type(move);
            char promo_char = 'q';

            switch (promo)
            {
                case KNIGHT:
                    promo_char = 'n';
                    break;
                case BISHOP:
                    promo_char = 'b';
                    break;
                case ROOK:
                    promo_char = 'r';
                    break;
                case QUEEN:
                    promo_char = 'q';
                    break;
                default:
                    break;
            }

            std::cout << promo_char;
        }
    }
} // anonymous namespace

namespace Perft
{
    /**
     * @brief Recursively counts leaf nodes at a target depth using bulk counting optimization.
     *
     * @param pos The current board position.
     * @param depth Remaining search depth in plies.
     * @return uint64_t Total number of leaf nodes reached.
     */
    Key run(Position& pos, int depth)
    {
        // Base case: at depth 0, return 1 for the current node.
        if (depth == 0)
        {
            return 1ULL;
        }

        MoveList move_list;
        MoveGen::generate_legal_moves(pos, move_list);

        // Bulk counting optimization: at depth 1, the number of legal moves equals the leaf node count.
        if (depth == 1)
        {
            return move_list.size();
        }

        Key nodes = 0ULL; // Accumulator for total leaf nodes across all branches.

        // Iterate through all generated legal moves using standard array index or pointers.
        for (int i = 0; i < move_list.size(); ++i)
        {
            Move move = move_list.moves[i]; // Access moves array directly from MoveList struct.

            pos.make_move(move);          // Apply the move to update the position state.
            nodes += run(pos, depth - 1); // Recursively search the subtree at depth - 1.
            pos.unmake_move(move);        // Revert the move to restore the position state.
        }

        return nodes;
    }

    /**
     * @brief Runs Perft from the root position and prints a detailed breakdown per move.
     *
     * @param pos The root board position to evaluate.
     * @param depth The target search depth in plies.
     */
    void divide(Position& pos, int depth)
    {
        if (depth <= 0)
        {
            return;
        }

        MoveList move_list;
        MoveGen::generate_legal_moves(pos, move_list);
        Key total_nodes = 0ULL; // Total leaf nodes generated across all root moves.

        auto start_time = std::chrono::high_resolution_clock::now(); // Start timer for benchmarking.

        // Process each root move individually.
        for (int i = 0; i < move_list.size(); ++i)
        {
            Move move = move_list.moves[i]; // Access moves array directly.

            pos.make_move(move);             // Apply root move.
            Key nodes = run(pos, depth - 1); // Count subtree nodes for this move.
            pos.unmake_move(move);           // Restore board position.

            total_nodes += nodes; // Accumulate total node count.

            // Output move notation and node count.
            print_uci_move(move);
            std::cout << ": " << nodes << "\n";
        }

        auto end_time = std::chrono::high_resolution_clock::now(); // Stop timer.
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Calculate Nodes Per Second (NPS).
        Key nps = (duration_ms > 0) ? (total_nodes * 1000 / duration_ms) : 0;

        // Print final summary report.
        std::cout << "\n====================================\n";
        std::cout << "Total Nodes : " << total_nodes << "\n";
        std::cout << "Time Elapsed: " << duration_ms << " ms\n";
        std::cout << "NPS         : " << nps << " nodes/sec\n";
        std::cout << "====================================\n";
    }
} // namespace Perft