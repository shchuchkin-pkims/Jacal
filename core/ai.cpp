#include "ai.h"
#include "rules.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace AI {

// Manhattan distance
static int dist(Coord a, Coord b) {
    return std::abs(a.row - b.row) + std::abs(a.col - b.col);
}

// Distance from a position to the nearest boarding tile of a ship
static int distToShip(const GameState& state, Coord pos, Team team) {
    auto tiles = state.boardingTiles(team);
    int best = 999;
    for (auto& t : tiles) {
        int d = dist(pos, t);
        if (d < best) best = d;
    }
    // Also consider the ship position directly
    int d = dist(pos, state.ships[static_cast<int>(team)].pos);
    if (d < best) best = d;
    return best;
}

int evaluateState(const GameState& state, Team team) {
    int score = 0;
    int ti = static_cast<int>(team);

    // Scored coins are the primary objective
    score += state.scores[ti] * 10000;

    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = state.pirates[ti][i];

        switch (p.state) {
        case PirateState::OnBoard: {
            // Being on the board is good (exploring)
            score += 100;

            // Carrying coin: value depends on distance to ship
            if (p.carryingCoin) {
                int d = distToShip(state, p.pos, team);
                score += 3000 - d * 200; // closer to ship = better
            }
            if (p.carryingGalleon) {
                int d = distToShip(state, p.pos, team);
                score += 9000 - d * 400;
            }

            // Spinner progress
            if (p.spinnerProgress > 0) {
                auto& tile = state.tileAt(p.pos);
                if (isSpinner(tile.type)) {
                    int required = spinnerSteps(tile.type);
                    // Being close to finishing a spinner is slightly better
                    score += p.spinnerProgress * 10;
                    // Penalty for being stuck on a long spinner
                    score -= (required - p.spinnerProgress) * 20;
                }
            }
            break;
        }
        case PirateState::OnShip:
            // Pirate on ship: slightly worse than on board (not exploring)
            score += 50;
            break;
        case PirateState::Dead:
            score -= 500;
            break;
        case PirateState::InTrap:
            score -= 300;
            break;
        case PirateState::InCave:
            score -= 200;
            break;
        }
    }

    // Bonus for having coins on nearby revealed tiles
    for (int r = 1; r <= 11; r++) {
        for (int c = 1; c <= 11; c++) {
            Coord cc = {r, c};
            if (!isLand(cc)) continue;
            auto& tile = state.tileAt(cc);
            if (tile.revealed && tile.coins > 0) {
                // Find nearest pirate
                int minDist = 999;
                for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                    auto& p = state.pirates[ti][i];
                    if (p.state == PirateState::OnBoard && !p.carryingCoin && !p.carryingGalleon) {
                        int d = dist(p.pos, cc);
                        if (d < minDist) minDist = d;
                    }
                }
                if (minDist < 999) {
                    score += tile.coins * (200 - minDist * 15);
                }
            }
        }
    }

    return score;
}

int evaluateMove(const GameState& state, const Move& move, Team team) {
    int bonus = 0;

    switch (move.type) {
    case MoveType::BoardShip: {
        // HUGE bonus for loading coins onto ship
        auto& p = state.pirateRef(move.pirateId);
        if (p.carryingCoin) bonus += 8000;
        if (p.carryingGalleon) bonus += 24000;
        break;
    }
    case MoveType::DisembarkPirate:
        // Good to get pirates on the island
        bonus += 200;
        break;

    case MoveType::MovePirate: {
        auto& p = state.pirateRef(move.pirateId);

        // Exploring unrevealed tiles
        if (isLand(move.to) && !state.tileAt(move.to).revealed) {
            if (!p.carryingCoin && !p.carryingGalleon)
                bonus += 300; // exploration is valuable
        }

        // Moving toward ship with coin
        if (p.carryingCoin || p.carryingGalleon) {
            int oldDist = distToShip(state, move.from, team);
            int newDist = distToShip(state, move.to, team);
            bonus += (oldDist - newDist) * 500; // reward getting closer
        }

        // Moving toward known coins
        if (!p.carryingCoin && !p.carryingGalleon && isLand(move.to)) {
            auto& tile = state.tileAt(move.to);
            if (tile.revealed && tile.coins > 0) {
                bonus += 2000; // pick up coin!
            }
            if (tile.revealed && tile.hasGalleonTreasure) {
                bonus += 6000;
            }
        }

        // Attacking enemies carrying coins
        if (isLand(move.to)) {
            auto enemies = state.enemyPiratesAt(move.to, team);
            for (auto& eid : enemies) {
                auto& enemy = state.pirateRef(eid);
                if (enemy.carryingCoin) bonus += 1500;
                if (enemy.carryingGalleon) bonus += 4500;
                bonus += 400; // attacking is generally good
            }
        }

        // Avoid known dangers (if tile is revealed)
        if (isLand(move.to) && state.tileAt(move.to).revealed) {
            TileType tt = state.tileAt(move.to).type;
            if (tt == TileType::Cannibal) bonus -= 5000;
            if (tt == TileType::Trap) bonus -= 1000;
            if (tt == TileType::RumBarrel) bonus -= 300;
        }
        break;
    }
    case MoveType::AdvanceSpinner:
        bonus += 50; // just advance
        break;

    case MoveType::ResurrectPirate:
        bonus += 600; // resurrecting is valuable
        break;

    case MoveType::SkipDrunk:
        bonus += 0;
        break;

    case MoveType::MoveShip: {
        // Move ship toward areas with our pirates carrying coins
        int ti = static_cast<int>(team);
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            auto& p = state.pirates[ti][i];
            if (p.state == PirateState::OnBoard && (p.carryingCoin || p.carryingGalleon)) {
                int oldD = dist(move.from, p.pos);
                int newD = dist(move.to, p.pos);
                bonus += (oldD - newD) * 200;
            }
        }
        break;
    }

    case MoveType::ChooseDirection:
    case MoveType::ChooseHorseDest:
    case MoveType::ChooseCaveExit:
        // For direction choices, prefer moves toward ship if carrying, else toward center
        if (move.to.valid()) {
            auto& p = state.pirateRef(move.pirateId);
            if (p.carryingCoin || p.carryingGalleon) {
                int d = distToShip(state, move.to, team);
                bonus -= d * 100; // closer to ship = better
            } else {
                // Prefer toward center of island (more unexplored)
                int centerDist = dist(move.to, {6, 6});
                bonus -= centerDist * 20;
            }

            // Avoid water
            if (isWater(move.to)) bonus -= 1000;
        }
        break;

    default:
        break;
    }

    return bonus;
}

Move chooseBestMove(const GameState& state, const MoveList& legalMoves) {
    if (legalMoves.empty()) return {};
    if (legalMoves.size() == 1) return legalMoves[0];

    Team team = state.currentTeam();
    Move bestMove = legalMoves[0];
    int bestScore = -9999999;

    for (auto& move : legalMoves) {
        // Quick heuristic score (no simulation)
        int score = evaluateMove(state, move, team);

        // 1-ply lookahead: simulate and evaluate result
        GameState copy = state;
        Rules::applyMove(copy, move);

        // Only evaluate if the turn actually completed (not in a choice phase)
        if (copy.phase == TurnPhase::ChooseAction) {
            score += evaluateState(copy, team);
        } else {
            // Mid-turn choice — just use the heuristic
            score += evaluateState(state, team);
        }

        // Small random factor to avoid predictability
        score += std::rand() % 50;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}

} // namespace AI
