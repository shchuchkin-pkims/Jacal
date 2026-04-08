#include "game.h"
#include "map_def.h"
#include <iostream>
#include <iomanip>
#include <cstring>

int main(int argc, char* argv[]) {
    uint32_t seed = 42;
    std::string mapId = "classic";
    int numTeams = 2;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--map") && i + 1 < argc) mapId = argv[++i];
        else if (!strcmp(argv[i], "--teams") && i + 1 < argc) numTeams = std::atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help")) {
            std::cout << "Usage: dump_board [--seed N] [--map ID] [--teams N]\n"
                      << "Maps: classic, duel, hangman, turtle, bridges\n";
            return 0;
        }
    }

    GameConfig cfg;
    cfg.seed = seed;
    cfg.mapId = mapId;
    cfg.numTeams = numTeams;

    Game game;
    game.newGame(cfg);
    auto& s = game.state();
    auto* map = findMap(mapId);

    std::cout << "Map: " << mapId << "  Seed: " << seed << "  Teams: " << numTeams
              << "  Land: " << map->countLandCells() << "\n\n";

    // Print terrain grid
    std::cout << "Terrain (L=land, .=sea, X=rock, 0-3=ship):\n";
    for (int r = 0; r < BOARD_SIZE; r++) {
        std::cout << std::setw(2) << r << " ";
        for (int c = 0; c < BOARD_SIZE; c++) {
            Coord co = {r, c};
            char ch = '.';
            if (s.mapIsLand(co)) ch = 'L';
            if (s.mapIsRock(co)) ch = 'X';
            for (int t = 0; t < numTeams; t++)
                if (s.ships[t].pos == co) ch = '0' + t;
            std::cout << ch;
        }
        std::cout << "\n";
    }

    // Print tile layout
    std::cout << "\nTile layout:\n";
    std::cout << "Row Col Type            Dir  TrVal Rum  Cave\n";
    std::cout << "--- --- --------------- ---- ----- ---- ----\n";
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (!s.mapIsLand({r, c})) continue;
            auto& t = s.board[r][c];
            std::cout << std::setw(3) << r << " "
                      << std::setw(3) << c << " "
                      << std::setw(15) << std::left << tileTypeName(t.type) << " "
                      << std::setw(4) << std::right << (int)t.directionBits << " "
                      << std::setw(5) << t.treasureValue << " "
                      << std::setw(4) << t.rumBottles << " "
                      << std::setw(4) << t.caveId << "\n";
        }

    // Summary stats
    int tileCount[static_cast<int>(TileType::COUNT)] = {};
    int totalCoins = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (!s.mapIsLand({r, c})) continue;
            tileCount[static_cast<int>(s.board[r][c].type)]++;
            totalCoins += s.board[r][c].treasureValue;
            if (s.board[r][c].type == TileType::Galleon) totalCoins += 3;
        }

    std::cout << "\nTile counts:\n";
    for (int i = 0; i < static_cast<int>(TileType::COUNT); i++) {
        if (tileCount[i] > 0)
            std::cout << "  " << tileTypeName(static_cast<TileType>(i))
                      << ": " << tileCount[i] << "\n";
    }
    std::cout << "Total potential coins: " << totalCoins << "\n";
    std::cout << "coinsRemaining(): " << Rules::coinsRemaining(s) << "\n";
    std::cout << "isGameOver(): " << (game.isGameOver() ? "true" : "false") << "\n";

    return 0;
}
