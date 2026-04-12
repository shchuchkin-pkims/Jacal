#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>

// ============================================================
// Constants
// ============================================================
constexpr int BOARD_SIZE = 13;
constexpr int ISLAND_SIZE = 11;
constexpr int LAND_TILES = 117;
constexpr int MAX_TEAMS = 4;
constexpr int PIRATES_PER_TEAM = 3;
constexpr int TOTAL_PIRATES = MAX_TEAMS * PIRATES_PER_TEAM;
constexpr int MAX_CHARACTERS = 5; // 3 BenGunn + 1 Missionary + 1 Friday
constexpr int TOTAL_COINS = 37;

// ============================================================
// Direction system (clockwise from North)
// ============================================================
enum Direction : int {
    DIR_N = 0, DIR_NE = 1, DIR_E = 2, DIR_SE = 3,
    DIR_S = 4, DIR_SW = 5, DIR_W = 6, DIR_NW = 7,
    DIR_COUNT = 8, DIR_NONE = -1
};

constexpr int DR[8] = {-1, -1,  0,  1,  1,  1,  0, -1};
constexpr int DC[8] = { 0,  1,  1,  1,  0, -1, -1, -1};

inline constexpr uint8_t dirBit(int d) { return static_cast<uint8_t>(1u << d); }

constexpr uint8_t DIRS_CARDINAL = dirBit(DIR_N) | dirBit(DIR_E) | dirBit(DIR_S) | dirBit(DIR_W);
constexpr uint8_t DIRS_DIAGONAL = dirBit(DIR_NE) | dirBit(DIR_SE) | dirBit(DIR_SW) | dirBit(DIR_NW);

inline uint8_t rotateDirBits(uint8_t bits, int quarters) {
    quarters = ((quarters % 4) + 4) % 4;
    int shift = quarters * 2;
    return static_cast<uint8_t>((bits << shift) | (bits >> (8 - shift)));
}

inline Direction dirFromDelta(int dr, int dc) {
    for (int d = 0; d < 8; d++)
        if (DR[d] == dr && DC[d] == dc) return static_cast<Direction>(d);
    return DIR_NONE;
}

inline Direction oppositeDir(Direction d) {
    return d == DIR_NONE ? DIR_NONE : static_cast<Direction>((d + 4) % 8);
}

inline int popcount8(uint8_t b) {
    int c = 0; while (b) { c += b & 1; b >>= 1; } return c;
}

// ============================================================
// Coordinate
// ============================================================
struct Coord {
    int row = -1, col = -1;
    bool operator==(const Coord& o) const { return row == o.row && col == o.col; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    bool operator<(const Coord& o) const { return row < o.row || (row == o.row && col < o.col); }
    Coord moved(Direction d) const {
        return d == DIR_NONE ? *this : Coord{row + DR[d], col + DC[d]};
    }
    bool inBounds() const { return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE; }
    bool valid() const { return row >= 0 && col >= 0; }
};

// ============================================================
// Board topology
// ============================================================
inline bool isCorner(Coord c) {
    return (c.row == 0 || c.row == 12) && (c.col == 0 || c.col == 12);
}
inline bool isWater(Coord c) {
    if (!c.inBounds() || isCorner(c)) return false;
    if (c.row == 0 || c.row == 12 || c.col == 0 || c.col == 12) return true;
    if ((c.row == 1 || c.row == 11) && (c.col == 1 || c.col == 11)) return true;
    return false;
}
inline bool isLand(Coord c) {
    return c.inBounds() && !isCorner(c) && !isWater(c);
}

// ============================================================
// Enums
// ============================================================
enum class TileType : int {
    Sea = 0, Empty, Arrow, Horse,
    Jungle, Desert, Swamp, Mountain, // spinners
    Ice, Trap, Crocodile, Cannibal,
    Fortress, ResurrectFort,
    Treasure, Galleon,
    Airplane, Lighthouse, Earthquake,
    Balloon, Cannon,
    BenGunn, Missionary, Friday,
    Rum, RumBarrel, Cave,
    ThickJungle, Grass, Caramba,
    COUNT
};

inline bool isSpinner(TileType t) {
    return t == TileType::Jungle || t == TileType::Desert ||
           t == TileType::Swamp  || t == TileType::Mountain;
}
inline int spinnerSteps(TileType t) {
    switch (t) {
        case TileType::Jungle:   return 2;
        case TileType::Desert:   return 3;
        case TileType::Swamp:    return 4;
        case TileType::Mountain: return 5;
        default: return 0;
    }
}

const char* tileTypeName(TileType t);
const char* tileSymbol(TileType t);

enum class Team : int { White = 0, Yellow = 1, Black = 2, Red = 3, None = -1 };

inline const char* teamName(Team t) {
    switch (t) {
        case Team::White:  return "Белые";
        case Team::Yellow: return "Жёлтые";
        case Team::Black:  return "Чёрные";
        case Team::Red:    return "Красные";
        default: return "—";
    }
}
inline const char* teamColor(Team t) {
    switch (t) {
        case Team::White:  return "#e0e0e0";
        case Team::Yellow: return "#f0d000";
        case Team::Black:  return "#303030";
        case Team::Red:    return "#d03030";
        default: return "#808080";
    }
}

enum class PirateState : int { OnShip, OnBoard, Dead, InTrap, InCave };

enum class CharacterType : int { BenGunn = 0, Missionary, Friday, None };

enum class TurnPhase : int {
    ChooseAction, ChooseArrowDirection, ChooseHorseDest,
    ChooseAirplaneTarget, ChooseLighthouseTiles,
    ChooseEarthquakeTiles, ChooseCaveExit, TurnComplete
};

enum class MoveType : int {
    MovePirate, MoveShip, DisembarkPirate, BoardShip,
    AdvanceSpinner, ResurrectPirate,
    ChooseDirection, ChooseHorseDest,
    ChooseAirplaneDest, ChooseLighthouseTiles,
    ChooseEarthquakeTiles, ChooseCaveExit,
    MoveCharacter, UseRum, SkipDrunk, PickupCoin
};

enum class EventType : int {
    TileRevealed, PirateMoved, PirateBoarded, PirateDisembarked,
    ShipMoved, CoinPlaced, CoinPickedUp, CoinDropped,
    CoinLoaded, CoinLost, GalleonPlaced,
    PirateAttacked, PirateDied, PirateResurrected,
    PirateTrapped, PirateFreed, PirateDrunk,
    CharacterJoined, CharacterMoved, CharacterDied,
    RumFound, RumUsed, AirplaneUsed, LighthouseUsed,
    EarthquakeTriggered, BalloonLiftoff, CannonFired,
    CaveEntered, CaveExited, TurnOrderChanged,
    SpinnerAdvanced, GameOver
};

// ============================================================
// Tile
// ============================================================
struct Tile {
    TileType type = TileType::Sea;
    bool revealed = false;
    uint8_t directionBits = 0;
    int coins = 0;
    bool hasGalleonTreasure = false;
    bool used = false;
    int rumBottles = 0;
    int caveId = -1;
    int visualVariant = 0;
    int rotation = 0;
    int treasureValue = 0;
};

// ============================================================
// Pirate
// ============================================================
struct PirateId {
    Team team = Team::None;
    int index = -1;
    bool operator==(const PirateId& o) const { return team == o.team && index == o.index; }
    bool operator!=(const PirateId& o) const { return !(*this == o); }
    bool valid() const { return team != Team::None && index >= 0; }
};

struct Pirate {
    PirateId id;
    PirateState state = PirateState::OnShip;
    Coord pos = {-1, -1};
    bool carryingCoin = false;
    bool carryingGalleon = false;
    int spinnerProgress = 0;
    int drunkTurnsLeft = 0;
    std::string name;
    bool isFemale = false;
    std::string portrait; // filename in assets/portraits/
};

// ============================================================
// Ship & Character
// ============================================================
struct Ship {
    Team team = Team::None;
    Coord pos = {-1, -1};
};

struct Character {
    CharacterType type = CharacterType::None;
    Team owner = Team::None;
    Coord pos = {-1, -1};
    bool discovered = false;
    bool alive = true;
    bool onShip = false;        // character is on the ship
    bool carryingCoin = false;
    bool carryingGalleon = false;
    bool convertedToPirate = false; // Missionary converted via rum → acts as regular pirate
};

// ============================================================
// Move
// ============================================================
struct Move {
    MoveType type = MoveType::MovePirate;
    PirateId pirateId;
    Coord from = {-1, -1};
    Coord to = {-1, -1};
    Direction chosenDir = DIR_NONE;
    int characterIndex = -1;
    // airplane
    PirateId airplaneTarget;
    Coord airplaneDest = {-1, -1};
    // lighthouse
    std::array<Coord, 4> lighthouseTiles = {};
    int lighthouseCount = 0;
    // earthquake
    std::array<Coord, 2> earthquakeTiles = {};
};

using MoveList = std::vector<Move>;

// ============================================================
// Game Event
// ============================================================
struct GameEvent {
    EventType type;
    Coord pos = {-1, -1};
    Coord pos2 = {-1, -1};
    PirateId pirateId;
    Team team = Team::None;
    TileType tileType = TileType::Sea;
    int value = 0;
};

using EventList = std::vector<GameEvent>;

// ============================================================
// Config
// ============================================================
struct GameConfig {
    int numTeams = 4;
    bool teamMode = false; // opposite teams are allies
    uint32_t seed = 0;
    std::string mapId = "classic"; // map to use

    // Tile density: 0.0 = almost all empty, 1.0 = no empty tiles
    // Default -1.0 = use original rules ratio (≈85% filled)
    float tileDensity = -1.0f;

    // Which team color indices participate. Empty = use 0..numTeams-1.
    // Example: {0, 3} means White and Red play (2 players, but colors White+Red).
    std::vector<int> teamSlots;

    // Sandbox mode: fill board with Empty, place one test tile at center
    bool sandbox = false;
    TileType sandboxTile = TileType::Empty;
    uint8_t sandboxDirBits = 0;    // for arrows/cannon direction
    int sandboxValue = 0;          // treasure value, rum count, etc.
};
