#include "ai.h"
#include "rules.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <queue>
#include <set>

namespace AI {

// ============================================================
// Helpers
// ============================================================

static int dist(Coord a, Coord b) {
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

// BFS shortest path length from 'start' to any coord in 'targets'
// Only walks through revealed, safe land tiles (avoids dangers)
static int bfsDistance(const GameState& state, Coord start, const std::vector<Coord>& targets, Team team) {
    if (targets.empty()) return 999;
    for (auto& t : targets)
        if (t == start) return 0;

    std::set<std::pair<int,int>> targetSet;
    for (auto& t : targets) targetSet.insert({t.row, t.col});

    bool visited[BOARD_SIZE][BOARD_SIZE] = {};
    std::queue<std::pair<Coord, int>> q;
    q.push({start, 0});
    visited[start.row][start.col] = true;

    while (!q.empty()) {
        auto [pos, d] = q.front(); q.pop();

        for (int dir = 0; dir < 8; dir++) {
            Coord next = pos.moved(static_cast<Direction>(dir));
            if (!next.inBounds() || visited[next.row][next.col]) continue;
            visited[next.row][next.col] = true;

            // Can walk to water if it's a target (boarding tile)
            if (targetSet.count({next.row, next.col}))
                return d + 1;

            // Only walk through land
            if (!state.mapIsLand(next)) continue;

            auto& tile = state.tileAt(next);
            // Can walk through unrevealed (unknown)
            if (!tile.revealed) {
                q.push({next, d + 1});
                continue;
            }
            // Avoid deadly tiles
            if (tile.type == TileType::Cannibal) continue;
            // Avoid traps if alone (no ally there)
            if (tile.type == TileType::Trap) {
                bool allyHere = false;
                auto pids = state.piratesAt(next, team);
                if (!pids.empty()) allyHere = true;
                if (!allyHere) continue;
            }
            q.push({next, d + 1});
        }
    }
    return 999;
}

// BFS distance to ship (any boarding tile or ship tile)
static int bfsToShip(const GameState& state, Coord pos, Team team) {
    auto boardingTiles = state.boardingTiles(team);
    // Add ship position itself for water-boarding
    boardingTiles.push_back(state.ships[static_cast<int>(team)].pos);
    return bfsDistance(state, pos, boardingTiles, team);
}

// Find coords of all visible coins on the map
static std::vector<Coord> findVisibleCoins(const GameState& state) {
    std::vector<Coord> coins;
    for (int r = 1; r <= 11; r++)
        for (int c = 1; c <= 11; c++) {
            Coord cc = {r, c};
            if (!state.mapIsLand(cc)) continue;
            auto& tile = state.tileAt(cc);
            if (tile.revealed && (tile.coins > 0 || tile.hasGalleonTreasure))
                coins.push_back(cc);
        }
    return coins;
}

// BFS distance to nearest visible coin
static int bfsToNearestCoin(const GameState& state, Coord pos, Team team) {
    auto coins = findVisibleCoins(state);
    if (coins.empty()) return 999;
    return bfsDistance(state, pos, coins, team);
}

static int piratesOnShip(const GameState& state, Team team) {
    int count = 0, ti = static_cast<int>(team);
    for (int i = 0; i < PIRATES_PER_TEAM; i++)
        if (state.pirates[ti][i].state == PirateState::OnShip) count++;
    return count;
}

static int piratesOnIsland(const GameState& state, Team team) {
    int count = 0, ti = static_cast<int>(team);
    for (int i = 0; i < PIRATES_PER_TEAM; i++)
        if (state.pirates[ti][i].state == PirateState::OnBoard) count++;
    return count;
}

static int revealedCount(const GameState& state) {
    int count = 0;
    for (int r = 1; r <= 11; r++)
        for (int c = 1; c <= 11; c++)
            if (state.mapIsLand({r,c}) && state.tileAt({r,c}).revealed) count++;
    return count;
}

// Check if ship front tile is known-dangerous
static bool isShipFrontDangerous(const GameState& state, Team team) {
    Coord front = state.shipFront(team);
    if (!front.valid() || !state.mapIsLand(front)) return true;
    auto& tile = state.tileAt(front);
    if (!tile.revealed) return false; // unknown = worth trying
    return tile.type == TileType::Cannibal ||
           tile.type == TileType::Crocodile ||
           tile.type == TileType::Cannon ||
           tile.type == TileType::Trap;
}

// ============================================================
// State evaluation
// ============================================================

int evaluateState(const GameState& state, Team team) {
    int score = 0;
    int ti = static_cast<int>(team);

    score += state.scores[ti] * 10000;

    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = state.pirates[ti][i];
        switch (p.state) {
        case PirateState::OnBoard: {
            score += 200;
            if (p.carryingCoin) {
                int d = bfsToShip(state, p.pos, team);
                score += 4000 - d * 300;
            }
            if (p.carryingGalleon) {
                int d = bfsToShip(state, p.pos, team);
                score += 12000 - d * 600;
            }
            if (p.spinnerProgress > 0) {
                auto& tile = state.tileAt(p.pos);
                if (isSpinner(tile.type))
                    score -= (spinnerSteps(tile.type) - p.spinnerProgress) * 30;
            }
            break;
        }
        case PirateState::OnShip:
            score -= 150; // penalty: should be exploring
            break;
        case PirateState::Dead:
            score -= 600;
            break;
        case PirateState::InTrap:
            score -= 400;
            break;
        case PirateState::InCave:
            score -= 300;
            break;
        }
    }

    // Bonus for accessible coins
    auto coins = findVisibleCoins(state);
    for (auto& cc : coins) {
        auto& tile = state.tileAt(cc);
        int coinVal = tile.coins + (tile.hasGalleonTreasure ? 3 : 0);
        // Find nearest free pirate
        int minDist = 999;
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            auto& p = state.pirates[ti][i];
            if (p.state == PirateState::OnBoard && !p.carryingCoin && !p.carryingGalleon)
                minDist = std::min(minDist, dist(p.pos, cc));
        }
        if (minDist < 999)
            score += coinVal * std::max(0, 250 - minDist * 25);
    }

    return score;
}

// ============================================================
// Move evaluation
// ============================================================

int evaluateMove(const GameState& state, const Move& move, Team team) {
    int bonus = 0;
    int ti = static_cast<int>(team);

    switch (move.type) {

    case MoveType::BoardShip: {
        auto& p = state.pirateRef(move.pirateId);
        if (p.carryingCoin) bonus += 10000;
        if (p.carryingGalleon) bonus += 30000;
        break;
    }

    case MoveType::DisembarkPirate: {
        int onIsland = piratesOnIsland(state, team);

        // Don't disembark if front is known-dangerous
        if (isShipFrontDangerous(state, team)) {
            bonus -= 2000; // strong disincentive
            break;
        }

        if (onIsland == 0) bonus += 3000;
        else if (onIsland == 1) bonus += 1000;
        else bonus += 300;
        break;
    }

    case MoveType::MovePirate: {
        auto& p = state.pirateRef(move.pirateId);

        // === CARRYING COIN: rush to ship ===
        if (p.carryingCoin || p.carryingGalleon) {
            int oldDist = bfsToShip(state, move.from, team);
            int newDist = bfsToShip(state, move.to, team);
            int improvement = oldDist - newDist;
            bonus += improvement * 1000;
            if (newDist == 0) bonus += 2000; // about to board!
            // Avoid going further from ship
            if (improvement < 0) bonus -= 500;
            break;
        }

        // === NOT CARRYING ===

        // Direct coin pickup
        if (state.mapIsLand(move.to)) {
            auto& tile = state.tileAt(move.to);
            if (tile.revealed && tile.coins > 0) bonus += 4000;
            if (tile.revealed && tile.hasGalleonTreasure) bonus += 10000;
        }

        // Moving toward nearest visible coin (BFS-based)
        {
            int oldCoinDist = bfsToNearestCoin(state, move.from, team);
            int newCoinDist = bfsToNearestCoin(state, move.to, team);
            if (oldCoinDist < 999 && newCoinDist < oldCoinDist)
                bonus += (oldCoinDist - newCoinDist) * 300;
            if (oldCoinDist < 999 && newCoinDist > oldCoinDist)
                bonus -= 100; // going away from coins
        }

        // Exploring unrevealed tiles
        if (state.mapIsLand(move.to) && !state.tileAt(move.to).revealed) {
            int rev = revealedCount(state);
            bonus += 500;
            if (rev < 30) bonus += 400; // early game: explore heavily
            else if (rev < 60) bonus += 200;
        }

        // Attacking enemies carrying coins
        if (state.mapIsLand(move.to)) {
            auto enemies = state.enemyPiratesAt(move.to, team);
            for (auto& eid : enemies) {
                auto& enemy = state.pirateRef(eid);
                if (enemy.carryingCoin) bonus += 2000;
                if (enemy.carryingGalleon) bonus += 6000;
                bonus += 300;
            }
        }

        // Avoid known dangers
        if (state.mapIsLand(move.to) && state.tileAt(move.to).revealed) {
            TileType tt = state.tileAt(move.to).type;
            if (tt == TileType::Cannibal) bonus -= 8000;
            if (tt == TileType::Trap) bonus -= 1000;
            if (tt == TileType::RumBarrel) bonus -= 200;
            if (tt == TileType::Crocodile) bonus -= 300;
        }
        break;
    }

    case MoveType::AdvanceSpinner:
        bonus += 150;
        break;

    case MoveType::ResurrectPirate:
        bonus += 2000;
        break;

    case MoveType::SkipDrunk:
        break;

    case MoveType::MoveShip: {
        // If pirates carrying coins, move ship toward them
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            auto& p = state.pirates[ti][i];
            if (p.state == PirateState::OnBoard && (p.carryingCoin || p.carryingGalleon)) {
                int oldD = dist(move.from, p.pos);
                int newD = dist(move.to, p.pos);
                bonus += (oldD - newD) * 500;
            }
        }

        // If all pirates on ship and front is dangerous, moving ship is good
        if (piratesOnShip(state, team) > 0 && isShipFrontDangerous(state, team)) {
            bonus += 1500; // move away from dangerous front

            // Check if NEW front tile is safer
            Coord newFront = {-1, -1};
            const int dirs[] = {0, 4, 2, 6, 1, 3, 5, 7};
            for (int d : dirs) {
                Coord adj = move.to.moved(static_cast<Direction>(d));
                if (adj.inBounds() && state.mapIsLand(adj)) { newFront = adj; break; }
            }
            if (newFront.valid()) {
                auto& tile = state.tileAt(newFront);
                if (!tile.revealed)
                    bonus += 500; // unknown front = promising
                else if (tile.type == TileType::Cannibal || tile.type == TileType::Crocodile)
                    bonus -= 800; // still dangerous — less good
                else
                    bonus += 800; // safe front!
            }
        }

        // Move toward unexplored areas if no coins visible
        if (findVisibleCoins(state).empty()) {
            // Prefer positions near unrevealed tiles
            int unrevNear = 0;
            for (int d = 0; d < 8; d++) {
                Coord adj = move.to.moved(static_cast<Direction>(d));
                if (adj.inBounds() && state.mapIsLand(adj) && !state.tileAt(adj).revealed)
                    unrevNear++;
            }
            bonus += unrevNear * 50;
        }
        break;
    }

    case MoveType::ChooseDirection:
    case MoveType::ChooseHorseDest:
    case MoveType::ChooseCaveExit:
        if (move.to.valid()) {
            auto& p = state.pirateRef(move.pirateId);
            if (p.carryingCoin || p.carryingGalleon) {
                int d = bfsToShip(state, move.to, team);
                bonus -= d * 200;
            } else {
                if (state.mapIsLand(move.to)) {
                    auto& tile = state.tileAt(move.to);
                    if (!tile.revealed) bonus += 300;
                    if (tile.revealed && tile.coins > 0) bonus += 2000;
                    if (tile.revealed && tile.hasGalleonTreasure) bonus += 5000;
                    if (tile.revealed && tile.type == TileType::Cannibal) bonus -= 8000;
                    if (tile.revealed && tile.type == TileType::Crocodile) bonus -= 300;
                }
                int centerDist = dist(move.to, {6, 6});
                bonus -= centerDist * 15;
            }
            if (state.mapIsWater(move.to)) bonus -= 3000;
        }
        break;

    case MoveType::UseRum:
        bonus += 400;
        break;

    case MoveType::MoveCharacter:
        if (move.to.valid() && state.mapIsLand(move.to)) {
            auto& tile = state.tileAt(move.to);
            if (!tile.revealed) bonus += 250;
            if (tile.revealed && tile.coins > 0) bonus += 1500;
            if (tile.revealed && tile.hasGalleonTreasure) bonus += 4000;
            if (tile.revealed && tile.type == TileType::Cannibal) bonus -= 5000;
        }
        break;

    default:
        break;
    }

    return bonus;
}

// ============================================================
// Move selection
// ============================================================

Move chooseBestMove(const GameState& state, const MoveList& legalMoves) {
    if (legalMoves.empty()) return {};
    if (legalMoves.size() == 1) return legalMoves[0];

    Team team = state.currentTeam();
    Move bestMove = legalMoves[0];
    int bestScore = -9999999;

    for (auto& move : legalMoves) {
        int score = evaluateMove(state, move, team);

        // 1-ply lookahead
        GameState copy = state;
        Rules::applyMove(copy, move);

        if (copy.phase == TurnPhase::ChooseAction) {
            score += evaluateState(copy, team);
        } else {
            score += evaluateState(state, team);
        }

        // Small random factor (reduced from 50 to 15 for more deterministic play)
        score += std::rand() % 15;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}

} // namespace AI
