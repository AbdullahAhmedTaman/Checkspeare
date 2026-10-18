<div align="center">

# ♟️ Checkspeare

**A high-performance chess engine and standalone graphical interface, written in modern C++23.**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23.html)
[![Protocol](https://img.shields.io/badge/Protocol-UCI-success.svg)](https://www.chessprogramming.org/UCI)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux-lightgrey)

[Features](#features) · [Performance](#performance) · [Playing Strength](#playing-strength) · [Building](#building) · [Usage](#usage) · [Architecture](#architecture) · [Documentation](#technical-documentation)

</div>

---

Checkspeare combines optimized bitboard move generation, alpha-beta search, and handcrafted position evaluation. The engine and the GUI are fully independent: the engine runs in any UCI-compatible chess GUI, and the graphical application can connect to any external UCI engine.

<p align="center">
  <img src="docs/images/gui-overview.png"
       alt="Checkspeare GUI: main window with the interactive chessboard and game controls"
       width="85%">
  <br>
  <sub><b>Checkspeare GUI</b>: main window with the interactive chessboard and game controls</sub>
</p>

<p align="center">
  <img src="docs/images/gameplay.gif"
       alt="Checkspeare GUI: gameplay demonstration with click-to-move piece movement"
       width="85%">
  <br>
  <sub><b>Checkspeare GUI</b>: gameplay demonstration with click-to-move piece movement</sub>
</p>

## Features

### Chess Engine

| Area | Details |
|---|---|
| **Move generation** | Bitboard representation with optimized attack calculation |
| **Sliding attacks** | BMI2/PEXT acceleration where supported; Hyperbola Quintessence fallback |
| **Search** | Alpha-beta with iterative deepening, aspiration windows, and quiescence search |
| **Search optimizations** | Transposition table, null-move pruning, late-move reductions (LMR), heuristic move ordering |
| **Evaluation** | Handcrafted and tapered: PeSTO piece-square tables, pawn structure, mobility, king safety |
| **Protocol** | UCI, for use with compatible GUIs and engine-testing tools |

### Graphical Interface

| Area | Details |
|---|---|
| **Board interaction** | Interactive chessboard with drag-and-drop or click-to-move input and smooth piece-movement animations |
| **Game controls** | In-app controls for managing games |
| **Opponents** | Play against Checkspeare engine, or connect any external UCI-compliant engine |
| **Presentation** | Custom visual styling with dedicated fonts, theme, and piece textures |
| **Audio** | In-game sound support |
| **Technology** | Built with **SFML** for windowing and rendering and **Dear ImGui** for the interface |

## Performance

| Benchmark | Result | Notes |
|---|---|---|
| **Perft** | 119,060,324 nodes | Start position, depth 6, 752 ms |
| **Search speed** | 5.29 M nodes/second | Start position, depth 18, 64 MB hash |

**Test environment:** AMD Ryzen 5 5600X · MinGW-w64 UCRT GCC 14.2.0 · Release build (`-O3`, IPO/LTO) · BMI2/PEXT sliding attacks enabled for both benchmarks. Results vary with hardware, compiler, and build configuration.

## Playing Strength

| Opponent | Games | W / D / L | Score | Elo Difference | LOS |
|---|---|---|---|---|---|
| Stockfish 19 *(`UCI_Elo` 2600)* | 100 | 55 / 11 / 34 | 60.5% | **+74.1 ± 66.3** | 98.7% |

*W / D / L is from Checkspeare's perspective; LOS is the likelihood of superiority.*

**Conditions:** cutechess-cli · 60 s + 0.6 s per game · `komodo.bin` opening book · 64 MB hash · Ryzen 5 5600X, 16 GB RAM, Windows 11.

> [!NOTE]
> Preliminary results from a single 100-game match with a wide error margin. Stockfish's `UCI_Elo` is not directly comparable to FIDE or rating-list Elo. Full methodology and the exact match command are in the [technical documentation](#technical-documentation).

## Building

### Requirements

| Requirement | Version / Notes |
|---|---|
| **Compiler** | C++23-capable GCC, Clang, or MSVC |
| **CMake** | 3.28 or newer |
| **Git** | Required by CMake to fetch dependencies |
| **Internet access** | Needed on the first configure to download dependencies |

SFML, Dear ImGui, ImGui-SFML, and Catch2 are downloaded and built automatically via [`FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html), so the first configure takes longer than later builds. Nothing needs to be installed manually.

### Get the Source

```bash
git clone https://github.com/AbdullahAhmedTaman/Checkspeare.git
cd Checkspeare
```

### Linux

SFML is built from source, so it needs its usual system development packages. On Debian/Ubuntu, for example:

```bash
sudo apt install build-essential cmake git \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libgl1-mesa-dev libfreetype-dev libopenal-dev libvorbis-dev libflac-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows

Install CMake, Git, and a supported compiler (Visual Studio with MSVC, or MinGW-w64), then:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## Usage

### Graphical Interface

Launch the Checkspeare GUI executable produced by the build. Its location depends on the selected generator and build configuration.

### Chess Engine

The Checkspeare engine can be used in two ways:

- **Through the Checkspeare GUI**, which launches and manages the engine for you.
- **Through any other UCI-compatible chess GUI**, such as [Arena](http://www.playwitharena.de/) or [Cute Chess](https://cutechess.com/). Add the engine executable to the GUI's engine list.

You can also run the engine directly in a terminal and send commands through standard input:

```text
uci
isready
position startpos
go depth 8
```

### Testing

```bash
ctest --test-dir build --output-on-failure
```

The suite includes perft validation and other engine checks.

## Architecture

Checkspeare consists of two independent components that communicate over the UCI protocol:

- **Engine (`engine/`)**: a standalone UCI executable responsible for board state, move generation, evaluation, and search.
- **GUI (`gui/`)**: an SFML/ImGui application responsible for rendering, user interaction, game management, and UCI engine communication.

```text
Checkspeare/
├── engine/
│   ├── src/
│   │   ├── board/       # Board representation and move generation
│   │   ├── eval/        # Position evaluation
│   │   ├── search/      # Search and move ordering
│   │   └── uci/         # UCI protocol handling
│   └── tests/           # Engine tests
├── gui/
│   ├── assets/          # Fonts, themes, and piece textures
│   ├── src/
│   │   ├── assets/      # Asset management
│   │   ├── audio/       # Audio handling
│   │   ├── controller/  # Application controllers
│   │   ├── engine/      # UCI subprocess communication
│   │   ├── game/        # Game and board logic
│   │   ├── input/       # User input handling
│   │   └── ui/          # ImGui interface components
│   └── tests/           # GUI tests
└── docs/                # Technical documentation and images
```

## Technical Documentation

For a detailed discussion of the architecture, bitboard optimizations, search algorithms, evaluation design, and implementation decisions, see the accompanying document:

**[Checkspeare Technical Documentation (PDF)](docs/Checkspeare-Technical-Documentation.pdf)**