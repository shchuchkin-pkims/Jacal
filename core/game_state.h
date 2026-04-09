#pragma once
#include "types.h"
#include "map_def.h"
#include <vector>
#include <random>

struct GameState {
    std::array<std::array<Tile, BOARD_SIZE>, BOARD_SIZE> board;
    std::array<std::array<Terrain, BOARD_SIZE>, BOARD_SIZE> terrain = {}; // from map
    std::array<std::array<Pirate, PIRATES_PER_TEAM>, MAX_TEAMS> pirates;
    std::array<Ship, MAX_TEAMS> ships;
    std::array<Character, MAX_CHARACTERS> characters;
    std::array<int, MAX_TEAMS> scores = {};
    std::array<int, MAX_TEAMS> rumOwned = {};

    // Map-aware topology
    bool mapIsLand(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Land;
    }
    bool mapIsWater(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Sea;
    }
    bool mapIsRock(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Rock;
    }

    int currentPlayerIndex = 0;
    std::array<int, MAX_TEAMS> turnOrder = {0, 1, 2, 3};
    int numActivePlayers = 4;
    TurnPhase phase = TurnPhase::ChooseAction;
    int turnNumber = 1;

    // Pending state for multi-step actions
    PirateId pendingPirate;
    Coord pendingPos = {-1, -1};
    uint8_t pendingDirBits = 0;
    std::vector<Coord> pendingChoices; // for horse, cave exit, etc.
    int lighthouseRemaining = 0;       // how many lighthouse tiles left to pick
    Coord earthquakeFirst = {-1, -1};  // first tile picked for earthquake swap
    int grassRoundsLeft = 0;          // when >0: each player controls next players team

    GameConfig config;

    // ===== Accessors =====
    Team currentTeam() const {
        return static_cast<Team>(turnOrder[currentPlayerIndex]);
    }
    Pirate& pirate(Team t, int i) { return pirates[static_cast<int>(t)][i]; }
    const Pirate& pirate(Team t, int i) const { return pirates[static_cast<int>(t)][i]; }

    Tile& tileAt(Coord c) { return board[c.row][c.col]; }
    const Tile& tileAt(Coord c) const { return board[c.row][c.col]; }

    // Ship queries
    int shipIndexAt(Coord c) const {
        for (int i = 0; i < MAX_TEAMS; i++)
            if (i < config.numTeams && ships[i].pos == c) return i;
        return -1;
    }
    bool isOwnShip(Coord c, Team team) const {
        return ships[static_cast<int>(team)].pos == c;
    }
    bool isAllyShip(Coord c, Team team) const {
        if (isOwnShip(c, team)) return true;
        if (config.teamMode && config.numTeams == 4) {
            Team ally = static_cast<Team>((static_cast<int>(team) + 2) % 4);
            return ships[static_cast<int>(ally)].pos == c;
        }
        return false;
    }
    bool isEnemyShip(Coord c, Team team) const {
        int idx = shipIndexAt(c);
        if (idx < 0) return false;
        return !isAllyShip(c, team);
    }

    // Ship front tile (first adjacent land tile — works for any map)
    Coord shipFront(Team team) const {
        Coord sp = ships[static_cast<int>(team)].pos;
        // Check 4 cardinal directions first (preferred), then diagonals
        const int dirs[] = {DIR_N, DIR_S, DIR_E, DIR_W, DIR_NE, DIR_SE, DIR_SW, DIR_NW};
        for (int d : dirs) {
            Coord adj = sp.moved(static_cast<Direction>(d));
            if (adj.inBounds() && mapIsLand(adj)) return adj;
        }
        return {-1, -1};
    }

    // Tiles from which a pirate can board a ship (all adjacent land tiles)
    std::vector<Coord> boardingTiles(Team team) const {
        std::vector<Coord> tiles;
        Coord sp = ships[static_cast<int>(team)].pos;
        for (int d = 0; d < 8; d++) {
            Coord adj = sp.moved(static_cast<Direction>(d));
            if (adj.inBounds() && mapIsLand(adj))
                tiles.push_back(adj);
        }
        return tiles;
    }

    bool hasPirateOnShip(Team team) const {
        int t = static_cast<int>(team);
        for (int i = 0; i < PIRATES_PER_TEAM; i++)
            if (pirates[t][i].state == PirateState::OnShip) return true;
        return false;
    }

    // Pirates at a position (all teams or specific team)
    std::vector<PirateId> piratesAt(Coord c, Team filter = Team::None) const {
        std::vector<PirateId> r;
        for (int t = 0; t < config.numTeams; t++) {
            if (filter != Team::None && static_cast<Team>(t) != filter) continue;
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = pirates[t][i];
                if (p.pos == c && p.state == PirateState::OnBoard) r.push_back(p.id);
            }
        }
        return r;
    }

    std::vector<PirateId> enemyPiratesAt(Coord c, Team myTeam) const {
        std::vector<PirateId> r;
        for (int t = 0; t < config.numTeams; t++) {
            Team tt = static_cast<Team>(t);
            if (tt == myTeam) continue;
            if (config.teamMode && config.numTeams == 4) {
                Team ally = static_cast<Team>((static_cast<int>(myTeam) + 2) % 4);
                if (tt == ally) continue;
            }
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = pirates[t][i];
                if (p.pos == c && p.state == PirateState::OnBoard) r.push_back(p.id);
            }
        }
        return r;
    }

    bool hasEnemyAt(Coord c, Team myTeam) const {
        return !enemyPiratesAt(c, myTeam).empty();
    }

    int alivePirateCount(Team team) const {
        int c = 0, t = static_cast<int>(team);
        for (int i = 0; i < PIRATES_PER_TEAM; i++)
            if (pirates[t][i].state != PirateState::Dead) c++;
        return c;
    }

    bool isPlayerActive(Team team) const {
        if (alivePirateCount(team) > 0) return true;
        for (auto& ch : characters)
            if (ch.owner == team && ch.discovered && ch.alive) return true;
        return false;
    }

    // Check if pirate is on a spinner and hasn't finished
    bool isOnActiveSpinner(const Pirate& p) const {
        if (p.state != PirateState::OnBoard || !isLand(p.pos)) return false;
        auto& tile = tileAt(p.pos);
        if (!tile.revealed || !isSpinner(tile.type)) return false;
        return p.spinnerProgress < spinnerSteps(tile.type);
    }

    Pirate& pirateRef(PirateId id) { return pirates[static_cast<int>(id.team)][id.index]; }
    const Pirate& pirateRef(PirateId id) const { return pirates[static_cast<int>(id.team)][id.index]; }

    void advanceTurn() {
        phase = TurnPhase::ChooseAction;
        pendingPirate = {};
        pendingPos = {-1, -1};
        pendingDirBits = 0;
        pendingChoices.clear();
        lighthouseRemaining = 0;
        earthquakeFirst = {-1, -1};

        // Grass (Voodoo) effect: decrement rounds
        if (grassRoundsLeft > 0) {
            grassRoundsLeft--;
            if (grassRoundsLeft == 0) {
                // Restore original turn order
                for (int i = 0; i < numActivePlayers; i++)
                    turnOrder[i] = i;
            }
        }

        for (int i = 0; i < numActivePlayers; i++) {
            currentPlayerIndex = (currentPlayerIndex + 1) % numActivePlayers;
            if (isPlayerActive(currentTeam())) {
                turnNumber++;
                // Handle drunk pirates
                int t = static_cast<int>(currentTeam());
                for (int j = 0; j < PIRATES_PER_TEAM; j++) {
                    if (pirates[t][j].drunkTurnsLeft > 0)
                        pirates[t][j].drunkTurnsLeft--;
                }
                return;
            }
        }
        turnNumber++;
    }
};

// Create shuffled deck of tiles (landCount = number of land cells on map)
std::vector<Tile> createDeck(uint32_t seed = 0, int landCount = LAND_TILES, float tileDensity = -1.0f);

// Initialize full game state
void initializeBoard(GameState& state, const GameConfig& config);
