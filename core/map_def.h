#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <array>

// Cell terrain type for custom maps
enum class Terrain : int { Sea = 0, Land = 1, Rock = 2 };

struct ShipSpawn {
    int team;   // 0-3
    int row, col;
};

struct MapDefinition {
    std::string id;          // "classic", "duel", etc.
    std::string name;        // Display name
    int minPlayers = 2;
    int maxPlayers = 4;
    std::array<std::array<Terrain, BOARD_SIZE>, BOARD_SIZE> terrain = {};
    std::vector<ShipSpawn> ships;

    bool cellIsLand(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Land;
    }
    bool cellIsWater(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Sea;
    }
    bool cellIsRock(Coord c) const {
        if (!c.inBounds()) return false;
        return terrain[c.row][c.col] == Terrain::Rock;
    }

    int countLandCells() const {
        int n = 0;
        for (int r = 0; r < BOARD_SIZE; r++)
            for (int c = 0; c < BOARD_SIZE; c++)
                if (terrain[r][c] == Terrain::Land) n++;
        return n;
    }
};

// Get all built-in maps
const std::vector<MapDefinition>& getBuiltinMaps();

// Find map by id (checks builtins + registered custom maps)
const MapDefinition* findMap(const std::string& id);

// Register custom maps so findMap() can locate them
void registerCustomMaps(const std::vector<MapDefinition>& maps);

// Custom map file I/O (.jmap text format)
bool saveMapToFile(const MapDefinition& map, const std::string& filepath);
bool loadMapFromFile(const std::string& filepath, MapDefinition& outMap);

// Load all custom maps from a directory
std::vector<MapDefinition> loadCustomMaps(const std::string& dirpath);
