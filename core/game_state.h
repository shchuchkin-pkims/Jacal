#pragma once
#include "types.h"
#include <vector>
#include <random>

struct GameState {
    std::array<std::array<Tile, BOARD_SIZE>, BOARD_SIZE> board;
    std::array<std::array<Pirate, PIRATES_PER_TEAM>, MAX_TEAMS> pirates;
    std::array<Ship, MAX_TEAMS> ships;
    std::array<Character, MAX_CHARACTERS> characters;
    std::array<int, MAX_TEAMS> scores = {};
    std::array<int, MAX_TEAMS> rumOwned = {};

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

    // Ship front tile (land tile directly in front)
    Coord shipFront(Team team) const {
        Coord sp = ships[static_cast<int>(team)].pos;
        if (sp.row == 0)  return {1, sp.col};
        if (sp.row == 12) return {11, sp.col};
        if (sp.col == 0)  return {sp.row, 1};
        if (sp.col == 12) return {sp.row, 11};
        return {-1, -1};
    }

    // Tiles from which a pirate can board a ship
    std::vector<Coord> boardingTiles(Team team) const {
        std::vector<Coord> tiles;
        Coord front = shipFront(team);
        if (!front.valid()) return tiles;
        tiles.push_back(front);
        Coord sp = ships[static_cast<int>(team)].pos;
        if (sp.row == 0 || sp.row == 12) {
            if (front.col > 1)  tiles.push_back({front.row, front.col - 1});
            if (front.col < 11) tiles.push_back({front.row, front.col + 1});
        } else {
            if (front.row > 1)  tiles.push_back({front.row - 1, front.col});
            if (front.row < 11) tiles.push_back({front.row + 1, front.col});
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

// Create shuffled deck of 117 land tiles
std::vector<Tile> createDeck(uint32_t seed = 0);

// Initialize full game state
void initializeBoard(GameState& state, const GameConfig& config);
