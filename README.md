# Jacal - Treasure Island

A digital adaptation of the "Jackal" board game. Pirates explore a mysterious island, searching for treasure and competing to load the most gold onto their ships.

## Features

- **Full game rules** implementation with 30 unique tile types
- **Qt6 QML GUI** with tile artwork and animated pirate tokens
- **Console version** (Windows/Linux) with ANSI color output
- **AI bot** with heuristic evaluation for single-player mode
- **Sandbox mode** for testing individual tile mechanics
- **2-4 players** with team and duel modes
- **Move log** with copy support for debugging

## Building

### Requirements

- CMake 3.16+
- C++17 compiler (GCC 11+, Clang 14+, MSVC 2019+)
- Qt 6.2+ with QML/Quick modules (for GUI version)

### Linux GUI

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./qml-client/jackal
```

### Console (no Qt needed)

```bash
g++ -std=c++17 -O2 -I core \
  core/types.cpp core/game_state.cpp core/rules.cpp \
  core/game.cpp core/ai.cpp console/main.cpp \
  -o jackal-console
./jackal-console
```

### Windows Console (cross-compile from Linux)

```bash
x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -I core \
  core/types.cpp core/game_state.cpp core/rules.cpp \
  core/game.cpp core/ai.cpp console/main.cpp \
  -o jackal-win64.exe
```

## Project Structure

```
Jacal/
  core/           - Game engine (pure C++17, no dependencies)
    types.h       - All enums, structs, constants
    game_state.h  - Board state, initialization, deck creation
    rules.h/cpp   - Move validation, tile effects, combat
    game.h        - Game controller
    ai.h/cpp      - Heuristic AI bot
  qml-client/     - Qt6 QML desktop application
    src/          - C++ bridge (GameController, BoardModel)
    qml/          - QML UI files
  console/        - Terminal-based version (Windows/Linux)
  assets/tiles/   - Tile artwork (50 images)
  .github/        - CI pipeline
```

## Game Modes

| Mode | Description |
|------|-------------|
| 1 vs AI (duel) | 2 ships, you vs AI bot |
| 2 players (duel) | 2 ships, hot-seat |
| 1 vs AI (teams) | 4 ships, you control 2 teams |
| 2-4 players | Hot-seat with 2-4 independent teams |
| Sandbox | Test individual tile types in isolation |

## Tile Types

30 unique tile types including: arrows (1/2/4 directions), horse (L-move), spinners (2-5 turns), ice, traps, crocodile, cannibal, fortress, treasure (I-V), galleon, balloon, cannon, caves, rum, and special characters (Ben Gunn, Missionary, Friday).

## License

This is a fan-made digital adaptation for educational purposes.
Jackal board game is originally by Alexander Skobelev.
