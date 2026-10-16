# Checkspeare

**Checkspeare** is a high-performance UCI chess engine and graphical user interface (GUI) written in modern C++. With an estimated playing strength of ~2700 Elo, it combines a highly optimized bitboard move generator with advanced search heuristics and a handcrafted evaluation function. 

The project is architected such that the engine and the graphical user interface are completely independent of each other. Checkspeare can be used as a standalone UCI engine with any standard chess GUI, and the included SFML/ImGui visual interface can be used to load and play against other UCI-compliant engines. Checkspeare is fully cross-platform and natively supports both Windows and Linux.

---
## Documentation

If you are interested in the internal workings of Checkspeare, a comprehensive documentation PDF is included in this repository. It provides a deep dive into the architecture, implementation details, and the complete development process behind both the chess engine and the graphical user interface.

---

## Directory Structure

The repository is divided into two primary components: the engine and the GUI.

```text
Checkspeare/
├── engine/              # Standalone UCI chess engine
│   ├── src/             # Core engine source files
│   │   ├── board/       # Bitboard representation and move generation
│   │   ├── eval/        # Handcrafted evaluation and PeSTO tables
│   │   ├── search/      # Alpha-Beta search, pruning, and heuristics
│   │   └── uci/         # Universal Chess Interface protocol handling
│   └── tests/           # Catch2 test suite (Perft, search validation, etc.)
└── gui/                 # Graphical User Interface
    ├── assets/          # Static assets for the application
    ├── src/             # GUI source files
    │   ├── assets/      # Asset management logic
    │   ├── audio/       # Sound effects and audio handling
    │   ├── controller/  # Application state controllers
    │   ├── engine/      # GUI-side engine communication wrappers
    │   ├── game/        # Visual chessboard and game logic
    │   ├── input/       # User input handling
    │   └── ui/          # ImGui interface components
    └── tests/           # GUI-specific test suite
```

---

## Main Features

### Engine Core & Search
*   **Move Generation:** Highly optimized bitboard representation utilizing mask-based legality checking and the Hyperbola Quintessence technique for sliding piece attacks.
*   **Search Algorithm:** Iterative Deepening with Fail-Soft Alpha-Beta (Negamax) pruning.
*   **Transposition Table:** Lockless, 4-way associative Zobrist hash table with a Two-Tier replacement strategy.
*   **Advanced Pruning & Reductions:**
    *   Null Move Pruning (NMP).
    *   Late Move Reductions (LMR) based on logarithmic scaling.
    *   Aspiration Windows for optimized root node resolution.
    *   Quiescence Search to resolve forcing tactical sequences and mitigate the horizon effect.
*   **Move Ordering:** MVV-LVA for captures, alongside Killer Move and History heuristics for quiet moves.

### Handcrafted Evaluation (HCE)
*   **Tapered Evaluation:** Smooth interpolation between middlegame and endgame piece values.
*   **PeSTO Piece-Square Tables:** Granular positional understanding for all piece types.
*   **Structural Knowledge:** Advanced pawn structure evaluation penalizing isolated/doubled pawns and rewarding passed pawns.
*   **King Safety & Mobility:** Dynamic pawn shield evaluation, knight outposts, and king-zone attack detection.

### Graphical User Interface
*   Integrated, lightweight visual interface for playing directly against the engine or analyzing positions.
*   Powered by **SFML** (windowing and rendering) and **ImGui** (immediate mode user interface).

---

## Dependencies

To build Checkspeare, your development environment must include:
*   **C++ Compiler:** Must support C++23 (GCC, Clang, or MSVC).
*   **CMake:** Version 3.28 or higher.
*   **SFML:** Simple and Fast Multimedia Library (required for the GUI).
*   **ImGui:** Dear ImGui (required for the GUI components).

---

## Installation & Building

Checkspeare uses CMake for a clean, out-of-source build process.

### Linux
- Install the required build tools and dependencies:
```bash
   sudo apt update
   sudo apt install build-essential cmake libsfml-dev

```

- Clone the repository and build:
```bash
git clone https://github.com/AbdullahAhmedTaman/Checkspeare
cd checkspeare
mkdir build && cd build
cmake ..
cmake --build . --config Release

```



### Windows

Ensure you have CMake installed and a compatible compiler (such as Visual Studio/MSVC or MinGW). SFML and ImGui should be accessible via your package manager (e.g., vcpkg) or included in your project structure.

- Open PowerShell or your Developer Command Prompt.
- Clone and build:
```powershell
git clone https://github.com/AbdullahAhmedTaman/Checkspeare
cd checkspeare
mkdir build
cd build
cmake ..
cmake --build . --config Release

```



---

## Usage

### Running the GUI

Launch the visual interface directly from the build directory:

```bash
# Linux
./CheckspeareGUI

# Windows
.\Release\CheckspeareGUI.exe

```

### Running the UCI Engine

Checkspeare is fully compliant with the Universal Chess Interface (UCI) protocol. You can run it via the command line or hook it into standard chess GUIs like Arena, Cutechess, or Encroissant.

```bash
# Start the UCI engine
./Checkspeare

# Example UCI commands:
uci
position startpos
go depth 8

```
