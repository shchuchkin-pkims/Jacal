#include "game_state.h"
#include <algorithm>
#include <random>

std::vector<Tile> createDeck(uint32_t seed) {
    std::vector<Tile> deck;
    deck.reserve(LAND_TILES);

    auto add = [&](TileType type, int count, int variant = 0) {
        for (int i = 0; i < count; i++) {
            Tile t; t.type = type; t.visualVariant = variant * 100 + i;
            deck.push_back(t);
        }
    };
    auto addArrow = [&](uint8_t dirs, int count) {
        for (int i = 0; i < count; i++) {
            Tile t; t.type = TileType::Arrow; t.directionBits = dirs;
            deck.push_back(t);
        }
    };
    auto addTreasure = [&](int value, int count) {
        for (int i = 0; i < count; i++) {
            Tile t; t.type = TileType::Treasure; t.treasureValue = value;
            deck.push_back(t);
        }
    };

    // Arrows (21)
    addArrow(dirBit(DIR_N), 3);
    addArrow(dirBit(DIR_NE), 3);
    addArrow(dirBit(DIR_E), 3);
    addArrow(dirBit(DIR_N) | dirBit(DIR_S), 3);
    addArrow(dirBit(DIR_N) | dirBit(DIR_E), 3);
    addArrow(DIRS_DIAGONAL, 3);
    addArrow(DIRS_CARDINAL, 3);

    // Horse (2)
    add(TileType::Horse, 2);

    // Spinners (12)
    add(TileType::Jungle, 5);
    add(TileType::Desert, 4);
    add(TileType::Swamp, 2);
    add(TileType::Mountain, 1);

    // Ice (6)
    add(TileType::Ice, 6);

    // Obstacles (8)
    add(TileType::Trap, 3);
    add(TileType::Crocodile, 4);
    add(TileType::Cannibal, 1);

    // Safe zones (3)
    add(TileType::Fortress, 2);
    add(TileType::ResurrectFort, 1);

    // Treasure (16 tiles = 37 coins)
    addTreasure(1, 5);
    addTreasure(2, 5);
    addTreasure(3, 3);
    addTreasure(4, 2);
    addTreasure(5, 1);

    // Galleon (1)
    { Tile t; t.type = TileType::Galleon; deck.push_back(t); }

    // Airplane (1)
    { Tile t; t.type = TileType::Airplane; deck.push_back(t); }

    // Caramba (1)
    { Tile t; t.type = TileType::Caramba; deck.push_back(t); }

    // Balloon (2)
    add(TileType::Balloon, 2);

    // Cannon (2) - base direction N, will be rotated
    for (int i = 0; i < 2; i++) {
        Tile t; t.type = TileType::Cannon; t.directionBits = dirBit(DIR_N);
        deck.push_back(t);
    }

    // Lighthouse (1)
    { Tile t; t.type = TileType::Lighthouse; deck.push_back(t); }

    // BenGunn (3)
    for (int i = 0; i < 3; i++) {
        Tile t; t.type = TileType::BenGunn; t.visualVariant = i;
        deck.push_back(t);
    }

    // Missionary (1)
    { Tile t; t.type = TileType::Missionary; deck.push_back(t); }

    // Friday (1)
    { Tile t; t.type = TileType::Friday; deck.push_back(t); }

    // Rum: 3 tiles × 3 bottles + 2 tiles × 2 bottles = 13 bottles
    for (int i = 0; i < 3; i++) {
        Tile t; t.type = TileType::Rum; t.rumBottles = 3; deck.push_back(t);
    }
    for (int i = 0; i < 2; i++) {
        Tile t; t.type = TileType::Rum; t.rumBottles = 2; deck.push_back(t);
    }

    // RumBarrel (4)
    add(TileType::RumBarrel, 4);

    // Cave (4)
    for (int i = 0; i < 4; i++) {
        Tile t; t.type = TileType::Cave; t.caveId = i; deck.push_back(t);
    }

    // Earthquake (1)
    { Tile t; t.type = TileType::Earthquake; deck.push_back(t); }

    // ThickJungle (3)
    add(TileType::ThickJungle, 3);

    // Grass (2)
    add(TileType::Grass, 2);

    // Fill remaining with Empty
    int remaining = LAND_TILES - static_cast<int>(deck.size());
    for (int i = 0; i < remaining; i++) {
        Tile t; t.type = TileType::Empty; t.visualVariant = i % 4;
        deck.push_back(t);
    }

    // Shuffle
    std::mt19937 rng(seed ? seed : std::random_device{}());
    std::shuffle(deck.begin(), deck.end(), rng);

    // Random rotation for directional tiles
    std::uniform_int_distribution<int> rotDist(0, 3);
    for (auto& tile : deck) {
        if (tile.type == TileType::Arrow || tile.type == TileType::Cannon) {
            int rot = rotDist(rng);
            tile.directionBits = rotateDirBits(tile.directionBits, rot);
            tile.rotation = rot;
        }
    }

    return deck;
}

void initializeBoard(GameState& state, const GameConfig& config) {
    state = GameState{};
    state.config = config;

    // Sea everywhere
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            state.board[r][c] = Tile{};
            state.board[r][c].type = TileType::Sea;
            state.board[r][c].revealed = true;
        }

    if (config.sandbox) {
        // === SANDBOX MODE ===
        // Fill all land with Empty (revealed), place test tiles near ships
        for (int r = 1; r <= 11; r++)
            for (int c = 1; c <= 11; c++) {
                if ((r == 1 || r == 11) && (c == 1 || c == 11)) continue;
                state.board[r][c].type = TileType::Empty;
                state.board[r][c].revealed = true; // all open
                state.board[r][c].visualVariant = (r + c) % 4;
            }

        // RNG for random directions in sandbox
        std::mt19937 sandboxRng(config.seed ? config.seed : std::random_device{}());
        std::uniform_int_distribution<int> rotDist(0, 3);
        std::uniform_int_distribution<int> dirDist(0, 7);
        int sandboxTileCounter = 0;

        auto placeSandbox = [&](int r, int c) {
            if (r < 1 || r > 11 || c < 1 || c > 11) return;
            if ((r == 1 || r == 11) && (c == 1 || c == 11)) return;
            Tile& t = state.board[r][c];
            t.type = config.sandboxTile;
            t.revealed = false;
            t.caveId = -1;
            t.rotation = 0;
            t.coins = 0; // NO coins on non-treasure tiles
            t.treasureValue = 0;
            t.rumBottles = 0;

            // Arrows/Cannon: randomize direction for each tile
            if (config.sandboxTile == TileType::Arrow) {
                uint8_t baseDirs = config.sandboxDirBits;
                int rot = rotDist(sandboxRng);
                t.directionBits = rotateDirBits(baseDirs, rot);
                t.rotation = rot;
            } else if (config.sandboxTile == TileType::Cannon) {
                int d = dirDist(sandboxRng);
                t.directionBits = dirBit(d);
                t.rotation = d / 2; // approximate rotation for image
            } else {
                t.directionBits = config.sandboxDirBits;
            }

            // Treasure: assign varying values
            if (config.sandboxTile == TileType::Treasure) {
                int vals[] = {1, 2, 3, 4, 5, 2, 3, 1, 4, 2, 3, 5, 1, 2};
                t.treasureValue = vals[sandboxTileCounter % 14];
            }

            // Rum: assign bottle count
            if (config.sandboxTile == TileType::Rum) {
                t.rumBottles = (sandboxTileCounter % 2 == 0) ? 3 : 2;
            }

            sandboxTileCounter++;
        };

        // Place test tiles near White ship: (2,5) (2,6) (2,7) (3,5) (3,6) (3,7) (4,6)
        placeSandbox(2, 5); placeSandbox(2, 6); placeSandbox(2, 7);
        placeSandbox(3, 5); placeSandbox(3, 6); placeSandbox(3, 7);
        placeSandbox(4, 6);

        // Near opponent ship (South)
        placeSandbox(10, 5); placeSandbox(10, 6); placeSandbox(10, 7);
        placeSandbox(9, 5); placeSandbox(9, 6); placeSandbox(9, 7);

        // Center area
        placeSandbox(5, 6); placeSandbox(6, 5); placeSandbox(6, 6);
        placeSandbox(6, 7); placeSandbox(7, 6);

        // For caves: assign IDs
        if (config.sandboxTile == TileType::Cave) {
            int cid = 0;
            for (int r = 1; r <= 11 && cid < 4; r++)
                for (int c = 1; c <= 11 && cid < 4; c++)
                    if (state.board[r][c].type == TileType::Cave)
                        state.board[r][c].caveId = cid++;
        }

        // Revealed treasure tiles far from test area (for testing coin pickup + delivery)
        // Placed at edges where they won't interfere with the tested tile type
        state.board[1][2].type = TileType::Treasure;
        state.board[1][2].treasureValue = 2;
        state.board[1][2].coins = 2;
        state.board[1][2].revealed = true;

        state.board[1][10].type = TileType::Treasure;
        state.board[1][10].treasureValue = 3;
        state.board[1][10].coins = 3;
        state.board[1][10].revealed = true;

    } else {
        // === NORMAL MODE ===
        auto deck = createDeck(config.seed);
        int idx = 0;
        for (int r = 1; r <= 11; r++)
            for (int c = 1; c <= 11; c++) {
                if ((r == 1 || r == 11) && (c == 1 || c == 11)) continue;
                if (idx < static_cast<int>(deck.size())) {
                    state.board[r][c] = deck[idx++];
                    state.board[r][c].revealed = false;
                }
            }
    }

    // Ships — placement depends on number of teams
    // 2 teams: face-off North vs South
    // 3 teams: N, E, S
    // 4 teams: N, E, S, W
    Coord shipPos[4] = {{0,6}, {6,12}, {12,6}, {6,0}};
    if (config.numTeams == 2 && !config.teamMode) {
        shipPos[0] = {0, 6};   // North
        shipPos[1] = {12, 6};  // South (face-off)
    }
    for (int t = 0; t < config.numTeams; t++) {
        state.ships[t] = {static_cast<Team>(t), shipPos[t]};
    }

    // Pirates
    for (int t = 0; t < config.numTeams; t++) {
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            Pirate& p = state.pirates[t][i];
            p.id = {static_cast<Team>(t), i};
            p.state = PirateState::OnShip;
            p.pos = shipPos[t];
        }
    }

    // Characters (undiscovered)
    for (int i = 0; i < 3; i++)
        state.characters[i] = {CharacterType::BenGunn, Team::None, {-1,-1}, false, true};
    state.characters[3] = {CharacterType::Missionary, Team::None, {-1,-1}, false, true};
    state.characters[4] = {CharacterType::Friday, Team::None, {-1,-1}, false, true};

    // Turn order
    state.numActivePlayers = config.numTeams;
    state.turnOrder = {0, 1, 2, 3};
    state.currentPlayerIndex = 0;
    state.phase = TurnPhase::ChooseAction;
    state.turnNumber = 1;
}
