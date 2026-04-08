#include "map_def.h"

static MapDefinition makeMap(const std::string& id, const std::string& name,
                              int minP, int maxP,
                              const char* grid[13],
                              const std::vector<ShipSpawn>& ships) {
    MapDefinition m;
    m.id = id;
    m.name = name;
    m.minPlayers = minP;
    m.maxPlayers = maxP;
    m.ships = ships;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            char ch = grid[r][c];
            if (ch == '#') m.terrain[r][c] = Terrain::Land;
            else if (ch == 'X') m.terrain[r][c] = Terrain::Rock;
            else m.terrain[r][c] = Terrain::Sea;
        }
    return m;
}

static std::vector<MapDefinition> buildMaps() {
    std::vector<MapDefinition> maps;

    // 1. CLASSIC (11x11 without corners)
    {
        const char* g[13] = {
            ".............",
            "..#########..",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            "..#########..",
            ".............",
        };
        maps.push_back(makeMap("classic", "Classic Island", 2, 4, g,
            {{0,0,6}, {1,6,12}, {2,12,6}, {3,6,0}}));
    }

    // 2. DUEL (1v1, wide island split by water channel)
    {
        const char* g[13] = {
            ".............",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".............",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".###########.",
            ".............",
        };
        maps.push_back(makeMap("duel", "Duel", 2, 2, g,
            {{0,0,6}, {1,12,6}}));
    }

    // 3. HANGMAN'S ISLAND (big island with lake in center)
    {
        const char* g[13] = {
            ".............",
            ".###########.",
            ".###########.",
            ".###.....###.",
            ".###.....###.",
            ".##.......##.",
            ".##.......##.",
            ".##.......##.",
            ".###.....###.",
            ".###.....###.",
            ".###########.",
            ".###########.",
            ".............",
        };
        // Ships on WATER adjacent to land (lake edges)
        maps.push_back(makeMap("hangman", "Hangman Island", 2, 4, g,
            {{0,0,6}, {1,6,12}, {2,12,6}, {3,6,0}}));
    }

    // 4. TURTLE ROCK (archipelago, narrow bridges, rock center)
    {
        const char* g[13] = {
            "..##.....##..",
            ".####...####.",
            ".####...####.",
            "..####.####..",
            "...##.#.##...",
            "......X......",
            ".##.#XXX#.##.",
            "......X......",
            "...##.#.##...",
            "..####.####..",
            ".####...####.",
            ".####...####.",
            "..##.....##..",
        };
        // Ships in water channels between islands (row 0/12 col 5 = sea, adjacent to land)
        maps.push_back(makeMap("turtle", "Turtle Rock", 2, 4, g,
            {{0,0,5}, {1,0,7}, {2,12,5}, {3,12,7}}));
    }

    // 5. TWO BRIDGES (spiral water channel inside island)
    {
        const char* g[13] = {
            ".............",
            ".###########.",
            ".#.........#.",
            ".#.#######.#.",
            ".#.#.....#.#.",
            ".#.#.###.#.#.",
            ".#...#...#.#.",
            ".#.#.#####.#.",
            ".#.#.......#.",
            ".#.#########.",
            ".#...........",
            ".###########.",
            ".............",
        };
        maps.push_back(makeMap("bridges", "Two Bridges", 2, 2, g,
            {{0,0,12}, {1,12,0}}));
    }

    return maps;
}

const std::vector<MapDefinition>& getBuiltinMaps() {
    static std::vector<MapDefinition> maps = buildMaps();
    return maps;
}

const MapDefinition* findMap(const std::string& id) {
    auto& maps = getBuiltinMaps();
    for (auto& m : maps)
        if (m.id == id) return &m;
    return maps.empty() ? nullptr : &maps[0];
}
