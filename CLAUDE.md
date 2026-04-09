# Jacal — Treasure Island

Digital adaptation of the "Шакал: Остров Сокровищ" (Jackal: Treasure Island) board game.

## Project Structure

```
Jacal/
├── core/                    # Pure C++17 game engine (NO Qt dependency)
│   ├── types.h/cpp          # Enums, structs (TileType, Team, Pirate, Move, GameEvent, etc.)
│   ├── game_state.h/cpp     # GameState struct, board init, deck creation, sandbox mode
│   ├── map_def.h/cpp        # Map definitions (Terrain enum, MapDefinition, builtin maps: "classic", "duel")
│   ├── rules.h/cpp          # Game rules engine (~1724 lines): getLegalMoves, applyMove, resolveChain
│   ├── game.h/cpp           # Game controller wrapper
│   └── ai.h/cpp             # Heuristic AI bot (1-ply lookahead)
│
├── qml-client/              # Qt6 QML GUI application
│   ├── main.cpp             # Entry point (registers GameController, BoardModel, NetworkClient, UpdateChecker)
│   ├── qml.qrc              # QML resource file
│   ├── src/
│   │   ├── gamecontroller.h/cpp  # Bridge between core and QML (moves, AI, selection, log, crew status)
│   │   ├── boardmodel.h/cpp      # QAbstractListModel for 13x13 board (tile images, rotation, spinner)
│   │   ├── gameserver.h/cpp      # TCP game server for multiplayer (lobby, slots, broadcast)
│   │   ├── networkclient.h/cpp   # TCP client (connect, chat, moves, LAN discovery)
│   │   ├── updatechecker.h/cpp   # GitHub release checker + downloader
│   │   └── protocol.h            # Network protocol constants (APP_VERSION "0.2.0", ports, slot states)
│   └── qml/
│       ├── main.qml              # Main window (start screen with tabs: single/sandbox/network, game over)
│       ├── GameBoard.qml         # Board grid + ships + pirate tokens + valid move highlights
│       ├── TileItem.qml          # Individual tile rendering (image + rotation + arrows + spinner progress)
│       ├── HUD.qml               # Right panel (turn, scores, crew status with portraits, move log, ship controls)
│       └── NetworkScreen.qml     # Multiplayer lobby UI
│
├── console/main.cpp         # Console version (ANSI colors, AI support, PickupCoin)
├── tests/test_core.cpp      # Unit tests for core engine
├── tools/dump_board.cpp      # Debug tool
├── assets/tiles/             # Tile images (extracted from print PDF + rules PDF)
├── temp_files/               # Task notes, portraits, additional tile extractions
│   ├── task.txt              # Current user task list (portraits, crew status, pirate names, etc.)
│   ├── portraits/            # Pirate portrait images by team color
│   ├── print-classic-addition/  # Additional tile images from expansion PDF
│   └── pirate_names.txt      # Random pirate names for assignment
├── .github/workflows/build.yml  # CI: test-core → build Linux GUI, Linux console, Windows console
├── CMakeLists.txt            # Root CMake
├── README.md                 # Project documentation
└── docs/GITHUB_SETUP.md      # GitHub repo setup instructions
```

## Remote Development

- **Build machine**: Ubuntu PC at `ssh -i /root/ubuntu.key user@home` (alias in /root/.ssh/config → 81.200.28.93:22)
- **Project path on remote**: `/home/user/Documents/VS_Code/Jacal/`
- **Build**: `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)`
- **Run GUI**: `./build/qml-client/jackal`
- **Test**: `./build/qml-client/jackal --platform offscreen` (exit 124 = OK, ran until timeout)
- Git repo initialized, 2 commits so far

## Key Architecture Decisions

### Board Coordinate System
- 13x13 grid. Map-based topology via `MapDefinition` (Terrain::Sea/Land/Rock)
- Classic map: 11x11 island minus 4 corners = 117 land tiles, water border
- Ships on water tiles, pirates on land. `shipFront()` finds nearest land tile

### Tile System
- 30 tile types in `TileType` enum (Caramba removal pending per user request)
- Each tile: type, directionBits (arrows/cannon), coins, treasureValue, rumBottles, revealed, used, rotation
- Tile images: `boardmodel.cpp::tileImagePath()` maps TileType → PNG filename
- Image rotation: `ImageRotationRole` computes angle to match direction bits

### Movement Chain
- `resolveChain()` handles arrow→ice→cannon chains with cycle detection (max 200 iterations)
- Multi-step: Arrow (multi-dir) → ChooseArrowDirection phase; Horse → ChooseHorseDest; Cave → ChooseCaveExit
- Spinner (Jungle 2 / Desert 3 / Swamp 4 / Mountain 5 turns): `isOnActiveSpinner()`, click-to-advance UX

### Coin Mechanics
- Auto-pickup on landing (`landOnTile`): pirate takes 1 coin if hands empty
- Carrying coin restricts to revealed tiles only (can't explore)
- Board ship → coin scored. Coin in water → lost

### AI Bot
- `AI::chooseBestMove()`: 1-ply lookahead + heuristic evaluation
- Priorities: score coins > pick up > explore > attack coin-carriers > avoid danger

### Sandbox Mode
- `GameConfig.sandbox = true` + `sandboxTile` type
- All land revealed as Empty, test tiles placed near ships and center
- Randomized arrow/cannon directions. Treasure tiles get varying values
- Test treasure at (1,2) and (1,10) for coin pickup testing

### Network (in progress)
- TCP server (`GameServer`) with lobby system (slots: Open/Closed/AI/Player)
- UDP broadcast for LAN discovery
- `NetworkClient` with connect/chat/move/ready
- `UpdateChecker` checks GitHub releases API

## Current State / Known Issues (from temp_files/task.txt)

### Pending Tasks from User:
1. Update tile images from `temp_files/print-classic-addition/` folder (rum bottles, earthquake, lighthouse, cave, jungle, grass, rum variants, Friday, Missionary, Ben Gunn, galleon)
2. Remove Caramba tile (useless in digital game)
3. Add random pirate names from `pirate_names.txt` (gender-aware for portraits)
4. Add crew status bar in HUD with pirate names, states, portraits, rum counter, rum-use button
5. Remove Horse tile darkening overlay — show normal tile image
6. Add pirate portraits from `temp_files/portraits/` folder (team-specific)

### Recently Fixed:
- Coin auto-pickup restored (was temporarily made explicit, reverted to auto per rules)
- Crocodile: fixed return to water/ship position
- Balloon: added clear message about pirate returning to ship
- Spinner: click-to-advance UX with progress display
- 2-team duel mode (North vs South)
- Arrow image rotation matches direction bits
- Tile images mapped to correct PDFs per user feedback

### Cannon Bug (reported, needs fix):
- Currently shoots pirate only 1 tile. Should shoot pirate ALL THE WAY to water (sea tiles) in cannon direction
- In water: pirate can only be rescued by own ship, gold sinks, enemy ship = death
