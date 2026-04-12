//
// Spectator stress-test: full AI-vs-AI games
// Emulates exactly the same AI loop as GameController::processAITurn()
// Checks for hangs, crashes, invariant violations
//
#include "game.h"
#include "rules.h"
#include "ai.h"
#include "map_def.h"
#include <iostream>
#include <set>
#include <chrono>
#include <cstdlib>

struct GameResult {
    std::string mapId;
    int numTeams;
    uint32_t seed;
    int turnsPlayed;
    bool completed;     // game over reached
    bool hung;          // AI couldn't make a move
    bool crashed;       // invariant violation
    int finalScores[4];
    std::string winner;
    double durationMs;
    std::string error;
    // Diagnostics for incomplete games
    int coinsOnBoard = 0;
    int coinsCarried = 0;
    int coinsUnrevealed = 0;
    int tilesRevealed = 0;
    int tilesTotal = 0;
    int piratesAlive = 0;
    int piratesDead = 0;
    int piratesOnShip = 0;
    int piratesTrapped = 0;
};

static GameResult playFullGame(const std::string& mapId, int numTeams, uint32_t seed, int maxTurns) {
    GameResult result = {};
    result.mapId = mapId;
    result.numTeams = numTeams;
    result.seed = seed;

    auto t0 = std::chrono::steady_clock::now();

    GameConfig cfg;
    cfg.mapId = mapId;
    cfg.numTeams = numTeams;
    cfg.seed = seed;

    Game game;
    game.newGame(cfg);

    int startCoins = Rules::coinsRemaining(game.state());
    int noProgressCounter = 0;
    int lastTurn = game.turnNumber();

    for (int turn = 0; turn < maxTurns; turn++) {
        if (game.isGameOver()) {
            result.completed = true;
            break;
        }

        auto moves = game.getLegalMoves();
        if (moves.empty()) {
            // No moves — try advancing turn
            game.state().advanceTurn();
            noProgressCounter++;
            if (noProgressCounter > numTeams * 3) {
                result.hung = true;
                result.error = "No moves for " + std::to_string(noProgressCounter) + " consecutive attempts";
                break;
            }
            continue;
        }
        noProgressCounter = 0;

        // AI chooses move (same as processAITurn)
        Move chosen = AI::chooseBestMove(game.state(), moves);
        game.makeMove(chosen);

        // Handle chain phases (arrow, horse, cave, airplane, lighthouse, earthquake)
        // Exact same logic as GameController::processAITurn with cycle detection
        int safety = 50;
        std::set<std::pair<int,int>> visited;
        while (game.currentPhase() != TurnPhase::ChooseAction &&
               game.currentPhase() != TurnPhase::TurnComplete && safety-- > 0) {
            auto phaseMoves = game.getLegalMoves();
            if (phaseMoves.empty()) break;

            Move pc = AI::chooseBestMove(game.state(), phaseMoves);

            // Cycle detection
            auto key = std::make_pair(pc.to.row, pc.to.col);
            if (visited.count(key)) {
                bool found = false;
                for (auto& alt : phaseMoves) {
                    auto altKey = std::make_pair(alt.to.row, alt.to.col);
                    if (!visited.count(altKey)) {
                        pc = alt;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    game.state().phase = TurnPhase::ChooseAction;
                    game.state().advanceTurn();
                    break;
                }
            }
            visited.insert(key);

            game.makeMove(pc);
        }

        if (safety <= 0 && game.currentPhase() != TurnPhase::ChooseAction) {
            game.state().phase = TurnPhase::ChooseAction;
            game.state().advanceTurn();
        }

        // Invariant checks
        int coins = Rules::coinsRemaining(game.state());
        if (coins > startCoins) {
            result.crashed = true;
            result.error = "Coins increased: " + std::to_string(coins) + " > " + std::to_string(startCoins);
            break;
        }

        for (int t = 0; t < numTeams; t++) {
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = game.state().pirates[t][i];
                if (p.state == PirateState::OnBoard && p.pos.valid() && !p.pos.inBounds()) {
                    result.crashed = true;
                    result.error = "Pirate out of bounds: team=" + std::to_string(t) +
                                   " idx=" + std::to_string(i) +
                                   " pos=(" + std::to_string(p.pos.row) + "," + std::to_string(p.pos.col) + ")";
                    break;
                }
            }
            if (result.crashed) break;
        }
        if (result.crashed) break;

        // Check for stuck game (turn number not advancing)
        if (game.turnNumber() == lastTurn) {
            noProgressCounter++;
            if (noProgressCounter > 20) {
                result.hung = true;
                result.error = "Turn number stuck at " + std::to_string(lastTurn) +
                               " for 20 iterations, phase=" + std::to_string(static_cast<int>(game.currentPhase()));
                break;
            }
        } else {
            noProgressCounter = 0;
            lastTurn = game.turnNumber();
        }

        result.turnsPlayed = game.turnNumber();
    }

    if (!result.completed && !result.hung && !result.crashed)
        result.completed = game.isGameOver();

    for (int t = 0; t < numTeams; t++)
        result.finalScores[t] = game.state().scores[t];

    if (result.completed || result.turnsPlayed > 0)
        result.winner = teamName(game.getWinner());

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result.turnsPlayed = game.turnNumber();

    // Diagnostics for incomplete games
    if (!result.completed && !result.hung && !result.crashed) {
        auto& s = game.state();
        result.coinsOnBoard = 0;
        result.coinsCarried = 0;
        result.coinsUnrevealed = 0;
        result.tilesRevealed = 0;
        result.tilesTotal = 0;
        result.piratesAlive = 0;
        result.piratesDead = 0;
        result.piratesOnShip = 0;
        result.piratesTrapped = 0;

        for (int r = 0; r < BOARD_SIZE; r++)
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (s.mapIsLand({r, c})) {
                    result.tilesTotal++;
                    auto& tile = s.board[r][c];
                    if (tile.revealed) {
                        result.tilesRevealed++;
                        result.coinsOnBoard += tile.coins;
                        if (tile.hasGalleonTreasure) result.coinsOnBoard += 3;
                    } else {
                        if (tile.type == TileType::Treasure)
                            result.coinsUnrevealed += tile.treasureValue;
                        if (tile.type == TileType::Galleon)
                            result.coinsUnrevealed += 3;
                    }
                }
            }

        for (int t = 0; t < numTeams; t++)
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = s.pirates[t][i];
                if (p.carryingCoin) result.coinsCarried++;
                if (p.carryingGalleon) result.coinsCarried += 3;
                switch (p.state) {
                    case PirateState::OnBoard: result.piratesAlive++; break;
                    case PirateState::OnShip: result.piratesOnShip++; break;
                    case PirateState::Dead: result.piratesDead++; break;
                    case PirateState::InTrap: result.piratesTrapped++; break;
                    case PirateState::InCave: result.piratesTrapped++; break;
                }
            }
    }

    return result;
}

int main() {
    std::cout << "=== Spectator Stress Test ===\n\n";

    struct TestCase {
        std::string mapId;
        int numTeams;
        int maxTurns;
    };

    // 10 games: mix of 2-player and 4-player, higher turn limits
    TestCase tests[] = {
        {"classic", 2, 2000},
        {"classic", 2, 2000},
        {"classic", 4, 3000},
        {"classic", 4, 3000},
        {"classic", 2, 2000},
        {"classic", 4, 3000},
        {"classic", 2, 2000},
        {"classic", 4, 3000},
        {"classic", 2, 2000},
        {"classic", 4, 3000},
    };
    int numTests = 10;

    int passed = 0, failed = 0;

    for (int i = 0; i < numTests; i++) {
        auto& tc = tests[i];
        uint32_t seed = static_cast<uint32_t>(i * 137 + 42);

        std::cout << "Game " << (i+1) << "/5: " << tc.mapId
                  << " " << tc.numTeams << "p seed=" << seed
                  << " max=" << tc.maxTurns << "t ... " << std::flush;

        auto result = playFullGame(tc.mapId, tc.numTeams, seed, tc.maxTurns);

        if (result.hung || result.crashed) {
            failed++;
            std::cout << "FAIL!\n";
            std::cout << "  Error: " << result.error << "\n";
            std::cout << "  Turns: " << result.turnsPlayed << "\n";
        } else {
            passed++;
            std::cout << "OK";
            if (result.completed)
                std::cout << " (finished at turn " << result.turnsPlayed << ")";
            else
                std::cout << " (" << result.turnsPlayed << " turns, ongoing)";

            std::cout << " [";
            for (int t = 0; t < tc.numTeams; t++) {
                if (t > 0) std::cout << " vs ";
                std::cout << result.finalScores[t];
            }
            std::cout << "]";

            if (result.completed)
                std::cout << " Winner: " << result.winner;

            std::cout << " (" << static_cast<int>(result.durationMs) << "ms)";
            std::cout << "\n";

            // Print diagnostics for incomplete games
            if (!result.completed) {
                std::cout << "  Diag: revealed=" << result.tilesRevealed << "/" << result.tilesTotal
                          << " coinsOnBoard=" << result.coinsOnBoard
                          << " coinsUnrevealed=" << result.coinsUnrevealed
                          << " coinsCarried=" << result.coinsCarried
                          << "\n";
                std::cout << "  Diag: alive=" << result.piratesAlive
                          << " onShip=" << result.piratesOnShip
                          << " dead=" << result.piratesDead
                          << " trapped=" << result.piratesTrapped
                          << "\n";
            }
        }
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
