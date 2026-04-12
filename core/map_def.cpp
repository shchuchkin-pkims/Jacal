#include "map_def.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

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

    return maps;
}

const std::vector<MapDefinition>& getBuiltinMaps() {
    static std::vector<MapDefinition> maps = buildMaps();
    return maps;
}

// Cache for custom maps loaded at runtime
static std::vector<MapDefinition> g_customMaps;

void registerCustomMaps(const std::vector<MapDefinition>& maps) {
    g_customMaps = maps;
}

const MapDefinition* findMap(const std::string& id) {
    // Check builtins first
    auto& maps = getBuiltinMaps();
    for (auto& m : maps)
        if (m.id == id) return &m;
    // Check custom maps
    for (auto& m : g_customMaps)
        if (m.id == id) return &m;
    return maps.empty() ? nullptr : &maps[0];
}

// ============================================================
// Custom map file I/O (.jmap text format)
// Format:
//   name=My Map
//   size=13
//   ship=0,0,6
//   ship=1,12,6
//   terrain:
//   .............
//   .###########.
//   ...
// ============================================================

bool saveMapToFile(const MapDefinition& map, const std::string& filepath) {
    FILE* f = fopen(filepath.c_str(), "w");
    if (!f) return false;

    fprintf(f, "name=%s\n", map.name.c_str());
    fprintf(f, "id=%s\n", map.id.c_str());
    fprintf(f, "size=%d\n", BOARD_SIZE);
    fprintf(f, "minPlayers=%d\n", map.minPlayers);
    fprintf(f, "maxPlayers=%d\n", map.maxPlayers);

    for (auto& sp : map.ships)
        fprintf(f, "ship=%d,%d,%d\n", sp.team, sp.row, sp.col);

    fprintf(f, "terrain:\n");
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            switch (map.terrain[r][c]) {
                case Terrain::Sea:  fputc('.', f); break;
                case Terrain::Land: fputc('#', f); break;
                case Terrain::Rock: fputc('X', f); break;
            }
        }
        fputc('\n', f);
    }

    fclose(f);
    return true;
}

bool loadMapFromFile(const std::string& filepath, MapDefinition& outMap) {
    FILE* f = fopen(filepath.c_str(), "r");
    if (!f) return false;

    outMap = MapDefinition{};
    char line[256];
    bool readingTerrain = false;
    int terrainRow = 0;

    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline
        int len = 0;
        while (line[len] && line[len] != '\n' && line[len] != '\r') len++;
        line[len] = 0;

        if (readingTerrain) {
            if (terrainRow < BOARD_SIZE) {
                for (int c = 0; c < BOARD_SIZE && c < len; c++) {
                    switch (line[c]) {
                        case '#': outMap.terrain[terrainRow][c] = Terrain::Land; break;
                        case 'X': outMap.terrain[terrainRow][c] = Terrain::Rock; break;
                        default:  outMap.terrain[terrainRow][c] = Terrain::Sea;  break;
                    }
                }
                terrainRow++;
            }
            continue;
        }

        std::string s(line);
        if (s.substr(0, 5) == "name=") outMap.name = s.substr(5);
        else if (s.substr(0, 3) == "id=") outMap.id = s.substr(3);
        else if (s.substr(0, 11) == "minPlayers=") outMap.minPlayers = atoi(s.substr(11).c_str());
        else if (s.substr(0, 11) == "maxPlayers=") outMap.maxPlayers = atoi(s.substr(11).c_str());
        else if (s.substr(0, 5) == "ship=") {
            ShipSpawn sp{};
            sscanf(s.substr(5).c_str(), "%d,%d,%d", &sp.team, &sp.row, &sp.col);
            outMap.ships.push_back(sp);
        }
        else if (s == "terrain:") readingTerrain = true;
    }

    fclose(f);
    if (outMap.id.empty()) outMap.id = outMap.name;
    return terrainRow > 0;
}

std::vector<MapDefinition> loadCustomMaps(const std::string& dirpath) {
    std::vector<MapDefinition> result;
    // Platform-independent directory listing using C
    #ifdef _WIN32
    std::string pattern = dirpath + "\\*.jmap";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            MapDefinition md;
            if (loadMapFromFile(dirpath + "\\" + fd.cFileName, md))
                result.push_back(md);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    #else
    // POSIX
    DIR* dir = opendir(dirpath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fname = entry->d_name;
            if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".jmap") {
                MapDefinition md;
                if (loadMapFromFile(dirpath + "/" + fname, md))
                    result.push_back(md);
            }
        }
        closedir(dir);
    }
    #endif
    return result;
}
