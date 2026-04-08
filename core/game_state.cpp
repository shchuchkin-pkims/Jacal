#include "game_state.h"
#include <algorithm>
#include <random>

std::vector<Tile> createDeck(uint32_t seed, int landCount) {
    std::vector<Tile> deck;
    deck.reserve(landCount);

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

    // Caramba removed — useless in computer game

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
    int remaining = landCount - static_cast<int>(deck.size());
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

    // Load map terrain
    const MapDefinition* mapDef = findMap(config.mapId);

    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            state.terrain[r][c] = mapDef->terrain[r][c];
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
                int rot = rotDist(sandboxRng); // 0-3 = 0°,90°,180°,270°
                t.directionBits = rotateDirBits(baseDirs, rot);
                t.rotation = rot;
                // For single-direction arrows: ensure only cardinal dirs (no diagonals)
                int ndirs = 0; uint8_t b = t.directionBits;
                while(b){ndirs+=b&1;b>>=1;}
                if (ndirs == 1) {
                    // Snap to nearest cardinal direction
                    for (int d=0;d<8;d++) {
                        if (t.directionBits & (1<<d)) {
                            int cardinals[]={0,0,2,2,4,4,6,6}; // snap diagonal->cardinal
                            t.directionBits = 1<<cardinals[d];
                            break;
                        }
                    }
                }
            } else if (config.sandboxTile == TileType::Cannon) {
                // Cannon fires only cardinal directions (N, E, S, W)
                const int cardinalDirs[] = {DIR_N, DIR_E, DIR_S, DIR_W};
                int d = cardinalDirs[rotDist(sandboxRng)]; // rotDist gives 0-3
                t.directionBits = dirBit(d);
                t.rotation = d / 2;
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

        // === Extra test tiles for specific sandbox maps ===
        auto placeExtra = [&](int r, int c, TileType type, bool revealed = true) {
            if (r < 1 || r > 11 || c < 1 || c > 11) return;
            if ((r == 1 || r == 11) && (c == 1 || c == 11)) return;
            Tile& t = state.board[r][c];
            t.type = type;
            t.revealed = revealed;
            t.coins = 0; t.treasureValue = 0; t.rumBottles = 0; t.directionBits = 0;
        };

        // ResurrectFort: add cannibal for killing pirates to test resurrection
        if (config.sandboxTile == TileType::ResurrectFort) {
            placeExtra(1, 4, TileType::Cannibal);
            placeExtra(1, 8, TileType::Cannibal);
        }

        // Rum: add traps, missionary, friday for testing interactions
        if (config.sandboxTile == TileType::Rum) {
            placeExtra(4, 5, TileType::Trap, false);
            placeExtra(4, 7, TileType::Trap, false);
            placeExtra(5, 5, TileType::Missionary, false);
            placeExtra(5, 7, TileType::Friday, false);
            placeExtra(7, 5, TileType::Cave, false); state.board[7][5].caveId = 0;
            placeExtra(7, 7, TileType::Cave, false); state.board[7][7].caveId = 1;
        }

        // Missionary: add Friday and Rum
        if (config.sandboxTile == TileType::Missionary) {
            placeExtra(4, 5, TileType::Friday, false);
            placeExtra(4, 7, TileType::Rum, false); state.board[4][7].rumBottles = 2;
        }

        // Friday: add rum, traps, spinners, cannibal, resurrect fort
        if (config.sandboxTile == TileType::Friday) {
            placeExtra(4, 4, TileType::Rum, false); state.board[4][4].rumBottles = 1;
            placeExtra(4, 5, TileType::Trap, false);
            placeExtra(4, 7, TileType::Jungle, false);
            placeExtra(4, 8, TileType::Cannibal, false);
            placeExtra(5, 4, TileType::ResurrectFort, false);
            placeExtra(5, 8, TileType::Fortress, false);
        }

        // Earthquake: add various non-earthquake tiles for swap testing
        if (config.sandboxTile == TileType::Earthquake) {
            placeExtra(4, 4, TileType::Treasure, false); state.board[4][4].treasureValue = 3;
            placeExtra(4, 8, TileType::Trap, false);
            placeExtra(8, 4, TileType::Jungle, false);
            placeExtra(8, 8, TileType::Balloon, false);
        }

        // Treasure tiles for general coin testing (far from test area)
        state.board[1][2].type = TileType::Treasure;
        state.board[1][2].treasureValue = 2;
        state.board[1][2].coins = 2;
        state.board[1][2].revealed = true;

        state.board[1][10].type = TileType::Treasure;
        state.board[1][10].treasureValue = 3;
        state.board[1][10].coins = 3;
        state.board[1][10].revealed = true;

    } else {
        // === NORMAL MODE — place tiles on all land cells ===
        int landCount = mapDef->countLandCells();
        auto deck = createDeck(config.seed, landCount);
        int idx = 0;
        for (int r = 0; r < BOARD_SIZE; r++)
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (state.terrain[r][c] == Terrain::Land) {
                    if (idx < static_cast<int>(deck.size())) {
                        state.board[r][c] = deck[idx++];
                        state.board[r][c].revealed = false;
                    }
                }
            }
    }

    // Ships — from map definition
    Coord shipPos[4] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1}};
    for (auto& sp : mapDef->ships) {
        if (sp.team >= 0 && sp.team < 4)
            shipPos[sp.team] = {sp.row, sp.col};
    }
    // Assign ships to teams (use first N from map)
    int shipIdx = 0;
    for (int t = 0; t < config.numTeams && shipIdx < static_cast<int>(mapDef->ships.size()); t++) {
        auto& sp = mapDef->ships[shipIdx++];
        shipPos[t] = {sp.row, sp.col};
    }
    for (int t = 0; t < config.numTeams; t++) {
        state.ships[t] = {static_cast<Team>(t), shipPos[t]};
    }

    // Pirate names pool (male / female)
    static const char* maleNames[] = {
        "Эдвард Титч", "Генри Морган", "Джек Рэкхем", "Барбаросса",
        "Сэмюэль Беллами", "Фрэнсис Дрейк", "Томас Тью", "Бартоломью Робертс",
        "Уильям Кидд", "Бен Эвери", "Франсуа Олоне", "Монбар",
        "Эдвард Лоу", "Чарльз Вейн", "Джон Хоули", "Джек Воробей",
        "Капитан Врунгель", "Джеймс Крюк", "Джон Сильвер", "Питер Блад",
        "Капитан Харлок", "Конрад", "Гектор Барбосса", "Дэви Джонс",
        "Джонни Лафитт", "Дэн Темпест", "Капитан Хук", "Эдвард Кенуэй",
        "Уилл Тернер", "Прихлоп Билл", "Джошами Гиббс", "Мистер Сми",
    };
    static const char* femaleNames[] = {
        "Энн Бонни", "Мадам Цзинь", "Мэри Рид", "Грейс О'Мэлли",
        "Сэйди Фарелл", "Анжелика", "Тиа Далма", "Кэнэри Робб",
    };
    static const int NUM_MALE = sizeof(maleNames) / sizeof(maleNames[0]);
    static const int NUM_FEMALE = sizeof(femaleNames) / sizeof(femaleNames[0]);

    // Portrait gender map: for each team, which pirate indices are female
    // white: 1=M,2=M,3=F,4=M  → index 2 is female
    // yellow: 1=M,2=F,3=M,4=M → index 1 is female
    // black: 1=M,2=F,3=M,4=F  → index 1 is female
    // red: 1=M,2=M,3=M,4=F    → all 3 pirates male (4th is spare)
    static const bool femaleMap[4][PIRATES_PER_TEAM] = {
        {false, false, true},   // white: pirate 0=M, 1=M, 2=F
        {false, true,  false},  // yellow: 0=M, 1=F, 2=M
        {false, true,  false},  // black: 0=M, 1=F, 2=M
        {false, false, false},  // red: 0=M, 1=M, 2=M
    };
    static const char* teamPrefix[] = {"white", "yellow", "black", "red"};

    std::mt19937 nameRng(config.seed ? config.seed + 777 : std::random_device{}());
    std::vector<int> malePool, femalePool;
    for (int i = 0; i < NUM_MALE; i++) malePool.push_back(i);
    for (int i = 0; i < NUM_FEMALE; i++) femalePool.push_back(i);
    std::shuffle(malePool.begin(), malePool.end(), nameRng);
    std::shuffle(femalePool.begin(), femalePool.end(), nameRng);
    int mi = 0, fi = 0;

    // Pirates
    for (int t = 0; t < config.numTeams; t++) {
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            Pirate& p = state.pirates[t][i];
            p.id = {static_cast<Team>(t), i};
            p.state = PirateState::OnShip;
            p.pos = shipPos[t];

            bool female = (t < 4) ? femaleMap[t][i] : false;
            p.isFemale = female;
            if (female && fi < static_cast<int>(femalePool.size())) {
                p.name = femaleNames[femalePool[fi++]];
            } else if (mi < static_cast<int>(malePool.size())) {
                p.name = maleNames[malePool[mi++]];
            }
            // Portrait: e.g. "white_1_male.png" (1-indexed)
            if (t < 4) {
                p.portrait = std::string(teamPrefix[t]) + "_" +
                    std::to_string(i + 1) + "_" +
                    (female ? "female" : "male") + ".png";
            }
        }
    }

    // Characters (undiscovered)
    for (int i = 0; i < 3; i++) {
        state.characters[i] = {};
        state.characters[i].type = CharacterType::BenGunn;
        state.characters[i].owner = Team::None;
        state.characters[i].pos = {-1,-1};
        state.characters[i].discovered = false;
        state.characters[i].alive = true;
    }
    state.characters[3] = {};
    state.characters[3].type = CharacterType::Missionary;
    state.characters[3].discovered = false;
    state.characters[3].alive = true;
    state.characters[4] = {};
    state.characters[4].type = CharacterType::Friday;
    state.characters[4].discovered = false;
    state.characters[4].alive = true;

    // Turn order
    state.numActivePlayers = config.numTeams;
    state.turnOrder = {0, 1, 2, 3};
    state.currentPlayerIndex = 0;
    state.phase = TurnPhase::ChooseAction;
    state.turnNumber = 1;
}
