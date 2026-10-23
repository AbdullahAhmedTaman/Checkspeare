<div align="center">

# ♟️ Checkspeare

**A high-performance chess engine and standalone graphical interface, written in modern C++23.**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23.html)
[![Protocol](https://img.shields.io/badge/Protocol-UCI-success.svg)](https://www.chessprogramming.org/UCI)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux-lightgrey.svg)

[Features](#features) · [Performance](#performance) · [Playing Strength](#playing-strength) · [Building](#building) · [Usage](#usage) · [Architecture](#project-structure) · [Documentation](#technical-documentation)

</div>

---

Checkspeare combines optimized bitboard move generation, alpha-beta search, and handcrafted position evaluation.

The engine and GUI are independent: the engine works with any UCI-compatible chess GUI, while the graphical interface supports human-vs-human, human-vs-engine, and engine-vs-engine games using any UCI-compatible engine.

<p align="center">
  <img src="docs/images/gui-overview.png" alt="Checkspeare graphical interface showing the chessboard and game controls" width="85%">
</p>

<p align="center">
  <img src="docs/images/gameplay.gif" alt="Checkspeare gameplay demonstration" width="85%">
</p>

## Features

### Chess Engine

- **Move generation:** Bitboards, BMI2/PEXT sliding attacks, and Hyperbola Quintessence fallback.
- **Search:** Alpha-beta, iterative deepening, aspiration windows, quiescence search, transposition table, null-move and futility pruning, late-move reductions, and check extensions.
- **Move ordering:** Static exchange evaluation (SEE), killer moves, history heuristic, countermoves, and continuation history.
- **Evaluation:** Handcrafted tapered evaluation with PeSTO piece-square tables, pawn structure, mobility, and king safety.
- **Opening book:** Polyglot `.bin` books with weighted-random or best-move selection.
- **Protocol:** UCI.

### Graphical Interface

- **Board:** Drag-and-drop and click-to-move input, smooth animations, and board arrows.
- **Game tools:** Chess clocks, custom time controls, SAN move list, FEN loading, evaluation bar, engine-thinking panel, and resignation.
- **Opponents:** Human-vs-human, human-vs-engine, and engine-vs-engine.
- **Presentation:** Custom themes, fonts, piece textures, and sound effects.
- **Technology:** SFML 3.1.0 for windowing and rendering, Dear ImGui for the interface, and ImGui-SFML for integration.

## Performance

- **Perft:** 119,060,324 nodes — starting position, depth 6, in 752 ms.
- **Search speed:** 5.11 million nodes/second — starting position, depth 18, 64 MB transposition table.

**Test environment:** AMD Ryzen 5 5600X · MinGW-w64 UCRT GCC 14.2.0 · Release build with `-O3` and IPO/LTO · PEXT enabled.

Results vary with hardware, compiler, and build configuration.

## Playing Strength

A 600-game match against Stockfish 19 configured with `UCI_Elo = 2900` (`SF2900`) produced the following results:

- **Estimated strength:** Approximately `2822` on this opponent's `UCI_Elo` scale.
- **Score:** 38%, with 35% draws.
- **Estimated deficit:** 78 Elo, according to BayesElo.

**Match conditions:** cutechess-cli · 60 seconds + 0.6 seconds increment · `komodo.bin` opening book · 64 MB hash · AMD Ryzen 5 5600X · 16 GB RAM · Windows 11.

Stockfish's `UCI_Elo` setting is not directly comparable to FIDE or rating-list Elo. The estimate describes performance under these specific match conditions, not a universally comparable rating.

See the [technical documentation](docs/Checkspeare-Technical-Documentation.pdf) for the methodology and match command.

## Building

### Requirements

- A C++23-compatible compiler: GCC, Clang, or MSVC.
- CMake 3.28 or newer.
- Git.
- An internet connection for the initial CMake configuration.

SFML, Dear ImGui, ImGui-SFML, and Catch2 are fetched and built automatically through CMake.

### Linux Dependencies

On Linux, SFML is built from source and uses system development libraries. On Debian/Ubuntu, install the required packages:

```bash
sudo apt update
sudo apt install build-essential cmake git \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libgl1-mesa-dev \
    libfreetype-dev libharfbuzz-dev \
    libflac-dev libvorbis-dev \
    libmbedtls-dev libssh2-1-dev
```

Check that the installed CMake version is at least 3.28. Some distributions provide an older version in their default repositories.

### Build Instructions

Clone the repository and build the project:

```bash
git clone https://github.com/AbdullahAhmedTaman/Checkspeare.git
cd Checkspeare

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For Visual Studio or another multi-config generator, omit `-DCMAKE_BUILD_TYPE=Release` during configuration and use `--config Release` when building.

By default, the build compiles the engine, GUI, and tests.

### Building Specific Targets

To build only one component:

```bash
# Engine only
cmake --build build --parallel --target CheckspeareEngine

# GUI only
cmake --build build --parallel --target CheckspeareGUI
```

For multi-config generators, add `--config Release` to these commands.

### Build Options

| Option | Default | Description |
|---|---|---|
| `USE_PEXT` | `ON` | Enables PEXT sliding attacks on supported x86-64 CPUs with BMI2. Use `-DUSE_PEXT=OFF` on CPUs without BMI2. |
| `ENGINE_X86_ARCH` | `x86-64-v3` | GCC/Clang architecture target for PEXT builds. Accepts `x86-64-v3`, `native`, or an empty value for `-mbmi2` only. |
| `ROOK_COMPRESSED` | `OFF` | Enables the compressed 16-bit rook attack table. Requires `USE_PEXT=ON`. |

## Usage

### Graphical Interface

Run `CheckspeareGUI` from `build/bin/`. Multi-config generators such as Visual Studio may place the executables in a configuration subdirectory.

In the setup dialog, select **Human** or **Engine** for each side and configure the time control. Engine players use `CheckspeareEngine` by default, but you can specify any other UCI-compatible engine.

### Chess Engine

Run `CheckspeareEngine` through the graphical interface or add it to a UCI-compatible GUI, such as [Arena](http://www.playwitharena.de/) or [Cute Chess](https://cutechess.com/).

The engine can also be controlled directly through standard input using UCI commands:

```text
uci
isready
position startpos
go depth 8
```

### UCI Options

| Option | Default | Description |
|---|---|---|
| `Hash` | `64` | Transposition table size in MB (1–32768). |
| `BookFile` | `book.bin` | Path to a Polyglot opening book. |
| `BookMode` | `Weighted` | `Weighted` selects moves randomly according to their book weights; `Best` selects the highest-weighted move. |

On startup, the engine attempts to load `book.bin` from its working directory. If the book is unavailable or does not contain the current position, the engine searches normally.

### Testing

Run the test suite with:

```bash
ctest --test-dir build -C Release --output-on-failure
```

## Project Structure

- **`engine/`** — Standalone UCI engine, including board representation, move generation, opening books, evaluation, search, and engine tests.
- **`gui/`** — SFML/ImGui graphical interface, game logic, assets, audio, UCI subprocess communication, and GUI tests.
- **`docs/`** — Technical documentation, screenshots, and gameplay demonstrations.

## Technical Documentation

For detailed explanations of the engine architecture, bitboard optimizations, search algorithms, evaluation design, and implementation decisions, see the accompanying document:

**[Checkspeare Technical Documentation (PDF)](docs/Checkspeare-Technical-Documentation.pdf)**