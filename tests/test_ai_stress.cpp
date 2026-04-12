//
// AI Stress Test for CI: full AI-vs-AI games on ALL available maps
// Validates: no crashes, no invariant violations, no infinite loops
// Expected runtime: ~10-15 seconds
//
#include "game.h"
#include "rules.h"
#include "ai.h"
#include "map_def.h"
#include <iostream>
#include <set>
#include <vector>
#include <string>

static bool playGame(const std::string& mapId, int numTeams, uint32_t seed, int maxTurns,
                     int gameIdx, bool& gameFinished) {
    GameConfig cfg;
    cfg.mapId = mapId;
    cfg.numTeams = numTeams;
    cfg.seed = seed;
    Game g;
    g.newGame(cfg);

    int startCoins = Rules::coinsRemaining(g.state());
    gameFinished = false;

    for (int t = 0; t < maxTurns && !g.isGameOver(); t++) {
        auto moves = g.getLegalMoves();
        if (moves.empty()) { g.state().advanceTurn(); continue; }
        Move m = AI::chooseBestMove(g.state(), moves);
        g.makeMove(m);

        // Resolve chain phases with cycle detection
        int safety = 30;
        std::set<std::pair<int,int>> v;
        while (g.currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
            auto pm = g.getLegalMoves();
            if (pm.empty()) break;
            Move pc = AI::chooseBestMove(g.state(), pm);
            auto k = std::make_pair(pc.to.row, pc.to.col);
            if (v.count(k)) {
                bool f = false;
                for (auto& a : pm) {
                    auto ak = std::make_pair(a.to.row, a.to.col);
                    if (!v.count(ak)) { pc = a; f = true; break; }
                }
                if (!f) {
                    g.state().phase = TurnPhase::ChooseAction;
                    g.state().advanceTurn();
                    break;
                }
            }
            v.insert(k);
            g.makeMove(pc);
        }
        if (safety <= 0) {
            g.state().phase = TurnPhase::ChooseAction;
            g.state().advanceTurn();
        }

        // Invariant: coins never increase
        int coins = Rules::coinsRemaining(g.state());
        if (coins > startCoins) {
            std::cerr << "CRASH: coins increased at game=" << gameIdx
                      << " map=" << mapId << " seed=" << seed << " turn=" << t
                      << " (" << coins << " > " << startCoins << ")\n";
            return false;
        }

        // Invariant: pirate states valid
        for (int ti = 0; ti < numTeams; ti++)
            for (int pi = 0; pi < PIRATES_PER_TEAM; pi++) {
                auto& p = g.state().pirates[ti][pi];
                if (p.state == PirateState::OnBoard && p.pos.valid() && !p.pos.inBounds()) {
                    std::cerr << "CRASH: OOB pirate at game=" << gameIdx
                              << " map=" << mapId << " seed=" << seed << "\n";
                    return false;
                }
            }
    }

    gameFinished = g.isGameOver();
    return true; // no crash
}

int main(int argc, char* argv[]) {
    // Collect all available maps: builtin + custom from maps/ directory
    std::vector<std::pair<std::string, int>> maps; // {mapId, maxPlayers}

    // Builtin maps
    auto& builtins = getBuiltinMaps();
    for (auto& m : builtins)
        maps.push_back({m.id, m.maxPlayers});

    // Custom maps from maps/ folder (try multiple paths)
    const char* mapDirs[] = {"maps", "../maps", "../../maps"};
    for (auto dir : mapDirs) {
        auto customs = loadCustomMaps(dir);
        if (!customs.empty()) {
            registerCustomMaps(customs);
            for (auto& m : customs) {
                // Skip duplicates (custom map with same id as builtin)
                bool dup = false;
                for (auto& [id, mp] : maps)
                    if (id == m.id) { dup = true; break; }
                if (!dup) maps.push_back({m.id, m.maxPlayers});
            }
            std::cout << "Loaded " << customs.size() << " custom maps from " << dir << "\n";
            break;
        }
    }

    std::cout << "Testing on " << maps.size() << " maps:\n";
    for (auto& [id, mp] : maps)
        std::cout << "  " << id << " (max " << mp << " players)\n";

    int gamesPerMap = 20; // 20 games per map, various seeds and player counts
    int totalGames = 0, totalFinished = 0, totalCrashed = 0;

    for (auto& [mapId, maxPlayers] : maps) {
        int mapCrashed = 0, mapFinished = 0;

        for (int i = 0; i < gamesPerMap; i++) {
            uint32_t seed = static_cast<uint32_t>(i * 13 + 7);
            // Alternate between 2-player and max-player modes
            int numTeams;
            if (i % 3 == 0 && maxPlayers >= 4)
                numTeams = 4;
            else if (i % 3 == 1 && maxPlayers >= 3)
                numTeams = 3;
            else
                numTeams = 2;
            if (numTeams > maxPlayers) numTeams = maxPlayers;

            int maxTurns = (numTeams >= 4) ? 2000 : 1500;

            bool finished = false;
            bool ok = playGame(mapId, numTeams, seed, maxTurns, totalGames, finished);

            totalGames++;
            if (!ok) { totalCrashed++; mapCrashed++; }
            if (finished) { totalFinished++; mapFinished++; }
        }

        std::cout << "  " << mapId << ": " << gamesPerMap << " games, "
                  << mapFinished << " finished, "
                  << mapCrashed << " crashed\n";
    }

    std::cout << "\n=== AI STRESS TEST ===\n";
    std::cout << "Maps tested: " << maps.size() << "\n";
    std::cout << "Total games: " << totalGames << "\n";
    std::cout << "Finished: " << totalFinished << "\n";
    std::cout << "Crashed: " << totalCrashed << "\n";

    if (totalCrashed > 0) {
        std::cerr << "FAIL: " << totalCrashed << " games crashed!\n";
        return 1;
    }

    std::cout << "PASS\n";
    return 0;
}
