#include "rules.h"
#include <set>
#include <algorithm>
#include <cmath>

namespace Rules {

// ============================================================
// Internal helpers
// ============================================================

// Horse (knight) move deltas: L-shaped
static const int HORSE_DR[] = {-2, -2, -1, -1,  1,  1,  2,  2};
static const int HORSE_DC[] = {-1,  1, -2,  2, -2,  2, -1,  1};
static const int HORSE_MOVES = 8;

static bool isAlly(Team a, Team b, const GameConfig& cfg) {
    if (a == b) return true;
    if (cfg.teamMode && cfg.numTeams == 4) {
        return (static_cast<int>(a) + 2) % 4 == static_cast<int>(b);
    }
    return false;
}

static bool canCarryInto(const Tile& tile) {
    // Can carry coin into this revealed tile?
    if (!tile.revealed) return false;
    if (tile.type == TileType::Fortress || tile.type == TileType::ResurrectFort)
        return false; // no gold in fortresses
    if (tile.type == TileType::ThickJungle) return false;
    return true;
}

// Get ship side direction (the direction from water toward the island center)
static Direction shipInwardDir(Coord shipPos) {
    if (shipPos.row == 0)  return DIR_S;
    if (shipPos.row == 12) return DIR_N;
    if (shipPos.col == 0)  return DIR_E;
    if (shipPos.col == 12) return DIR_W;
    return DIR_NONE;
}

// ============================================================
// PHASE-SPECIFIC MOVES (arrow choice, horse, etc.)
// ============================================================

static MoveList getMovesForPhase(const GameState& s) {
    MoveList moves;
    switch (s.phase) {
    case TurnPhase::ChooseArrowDirection: {
        // Offer each direction in pendingDirBits
        for (int d = 0; d < 8; d++) {
            if (s.pendingDirBits & dirBit(d)) {
                Move m;
                m.type = MoveType::ChooseDirection;
                m.pirateId = s.pendingPirate;
                m.from = s.pendingPos;
                m.to = s.pendingPos.moved(static_cast<Direction>(d));
                m.chosenDir = static_cast<Direction>(d);
                moves.push_back(m);
            }
        }
        break;
    }
    case TurnPhase::ChooseHorseDest: {
        for (auto& dest : s.pendingChoices) {
            Move m;
            m.type = MoveType::ChooseHorseDest;
            m.pirateId = s.pendingPirate;
            m.from = s.pendingPos;
            m.to = dest;
            moves.push_back(m);
        }
        break;
    }
    case TurnPhase::ChooseCaveExit: {
        for (auto& dest : s.pendingChoices) {
            Move m;
            m.type = MoveType::ChooseCaveExit;
            m.pirateId = s.pendingPirate;
            m.from = s.pendingPos;
            m.to = dest;
            moves.push_back(m);
        }
        break;
    }
    default:
        break;
    }
    return moves;
}

// ============================================================
// SHIP MOVES
// ============================================================

static void addShipMoves(const GameState& s, Team team, MoveList& moves) {
    if (!s.hasPirateOnShip(team)) return;
    const Ship& ship = s.ships[static_cast<int>(team)];
    Coord pos = ship.pos;

    // Ship moves 1 tile along its side
    Coord left, right;
    if (pos.row == 0) {        // North side
        left  = {0, pos.col - 1};
        right = {0, pos.col + 1};
    } else if (pos.row == 12) { // South side
        left  = {12, pos.col + 1};
        right = {12, pos.col - 1};
    } else if (pos.col == 0) {  // West side
        left  = {pos.row + 1, 0};
        right = {pos.row - 1, 0};
    } else if (pos.col == 12) { // East side
        left  = {pos.row - 1, 12};
        right = {pos.row + 1, 12};
    } else return;

    // Check bounds: ship can't go to corners or beyond col/row 1..11
    auto validShipPos = [](Coord c) -> bool {
        if (!c.inBounds()) return false;
        if (isCorner(c)) return false;
        // Must stay on border
        if (c.row != 0 && c.row != 12 && c.col != 0 && c.col != 12) return false;
        // Stay within 1..11 on the moving axis
        if (c.row == 0 || c.row == 12) return c.col >= 1 && c.col <= 11;
        return c.row >= 1 && c.row <= 11;
    };

    for (Coord dest : {left, right}) {
        if (!validShipPos(dest)) continue;
        // Can't move onto another ship
        if (s.shipIndexAt(dest) >= 0) continue;
        Move m;
        m.type = MoveType::MoveShip;
        m.pirateId = {team, -1};
        m.from = pos;
        m.to = dest;
        moves.push_back(m);
    }
}

// ============================================================
// PIRATE LAND MOVES
// ============================================================

static void addLandMoves(const GameState& s, const Pirate& p, MoveList& moves) {
    Team team = p.id.team;
    bool carrying = p.carryingCoin || p.carryingGalleon;

    for (int d = 0; d < 8; d++) {
        Coord to = p.pos.moved(static_cast<Direction>(d));
        if (!to.inBounds() || isCorner(to)) continue;

        // Water: normally can't jump in from land
        if (isWater(to)) {
            // Exception: boarding own/ally ship
            if (s.isAllyShip(to, team)) {
                Move m;
                m.type = MoveType::BoardShip;
                m.pirateId = p.id;
                m.from = p.pos;
                m.to = to;
                moves.push_back(m);
            }
            continue;
        }

        // Land tile
        const Tile& tile = s.tileAt(to);

        // Carrying gold: can only move to revealed tiles, can't enter fortresses
        if (carrying) {
            if (!tile.revealed) continue;
            if (tile.type == TileType::Fortress || tile.type == TileType::ResurrectFort) continue;
            if (tile.type == TileType::ThickJungle) continue;
        }

        // Fortress: can't enter if enemy is there
        if (tile.revealed && tile.type == TileType::Fortress && s.hasEnemyAt(to, team)) continue;

        // Enemy on tile: this is an attack (need empty hands to attack, unless dropping coin)
        // The rules allow dropping coin and attacking in one move
        // We generate the move - applyMove handles the coin drop

        Move m;
        m.type = MoveType::MovePirate;
        m.pirateId = p.id;
        m.from = p.pos;
        m.to = to;
        moves.push_back(m);
    }

    // Can also board ship from boarding positions
    for (int t = 0; t < s.config.numTeams; t++) {
        Team st = static_cast<Team>(t);
        if (!isAlly(team, st, s.config)) continue;
        auto boardingTiles = s.boardingTiles(st);
        for (auto& bt : boardingTiles) {
            if (bt == p.pos) {
                Coord shipPos = s.ships[t].pos;
                // Already handled above if ship is adjacent
                // But diagonal boarding might not be adjacent in all cases
                // Check if not already added
                bool found = false;
                for (auto& mv : moves) {
                    if (mv.type == MoveType::BoardShip && mv.to == shipPos) { found = true; break; }
                }
                if (!found) {
                    Move m;
                    m.type = MoveType::BoardShip;
                    m.pirateId = p.id;
                    m.from = p.pos;
                    m.to = shipPos;
                    moves.push_back(m);
                }
            }
        }
    }
}

// ============================================================
// SWIMMING MOVES
// ============================================================

static void addSwimmingMoves(const GameState& s, const Pirate& p, MoveList& moves) {
    Team team = p.id.team;
    // Can swim to adjacent water tiles along coast, or exit to land
    for (int d = 0; d < 8; d++) {
        Coord to = p.pos.moved(static_cast<Direction>(d));
        if (!to.inBounds() || isCorner(to)) continue;

        if (isWater(to)) {
            // Can swim to adjacent water (can't carry gold while swimming)
            if (s.isAllyShip(to, team)) {
                Move m;
                m.type = MoveType::BoardShip;
                m.pirateId = p.id;
                m.from = p.pos;
                m.to = to;
                moves.push_back(m);
            } else if (s.shipIndexAt(to) < 0) {
                // Empty water tile
                Move m;
                m.type = MoveType::MovePirate;
                m.pirateId = p.id;
                m.from = p.pos;
                m.to = to;
                moves.push_back(m);
            }
            // Enemy ship = death, don't generate as valid move
        } else if (isLand(to)) {
            // Exit water onto land
            Move m;
            m.type = MoveType::MovePirate;
            m.pirateId = p.id;
            m.from = p.pos;
            m.to = to;
            moves.push_back(m);
        }
    }
}

// ============================================================
// DISEMBARK
// ============================================================

static void addDisembarkMoves(const GameState& s, const Pirate& p, MoveList& moves) {
    Coord front = s.shipFront(p.id.team);
    if (!front.valid() || !isLand(front)) return;

    // Pirates on ship never carry gold (it's auto-loaded)
    Move m;
    m.type = MoveType::DisembarkPirate;
    m.pirateId = p.id;
    m.from = s.ships[static_cast<int>(p.id.team)].pos;
    m.to = front;
    moves.push_back(m);
}

// ============================================================
// SPINNER ADVANCE
// ============================================================

static void addSpinnerMoves(const GameState& s, const Pirate& p, MoveList& moves) {
    // Can advance one step on the spinner
    Move m;
    m.type = MoveType::AdvanceSpinner;
    m.pirateId = p.id;
    m.from = p.pos;
    m.to = p.pos;
    moves.push_back(m);
}

// ============================================================
// RESURRECTION
// ============================================================

static void addResurrectionMoves(const GameState& s, Team team, MoveList& moves) {
    int t = static_cast<int>(team);
    // Need a live pirate at resurrection fort
    bool hasPirateAtFort = false;
    Coord fortPos = {-1, -1};
    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = s.pirates[t][i];
        if (p.state == PirateState::OnBoard && isLand(p.pos)) {
            auto& tile = s.tileAt(p.pos);
            if (tile.revealed && tile.type == TileType::ResurrectFort) {
                hasPirateAtFort = true;
                fortPos = p.pos;
                break;
            }
        }
    }
    if (!hasPirateAtFort) return;

    // Can resurrect dead pirates (one per turn)
    // Can't resurrect if already have 3+ combatants (pirates + characters)
    int combatants = s.alivePirateCount(team);
    for (auto& ch : s.characters)
        if (ch.owner == team && ch.discovered && ch.alive) combatants++;
    if (combatants >= PIRATES_PER_TEAM) return;

    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        if (s.pirates[t][i].state == PirateState::Dead) {
            Move m;
            m.type = MoveType::ResurrectPirate;
            m.pirateId = {team, i};
            m.from = fortPos;
            m.to = fortPos;
            moves.push_back(m);
        }
    }
}

// ============================================================
// GET LEGAL MOVES (main entry)
// ============================================================

MoveList getLegalMoves(const GameState& state) {
    if (state.phase != TurnPhase::ChooseAction) {
        return getMovesForPhase(state);
    }

    MoveList moves;
    Team team = state.currentTeam();
    int t = static_cast<int>(team);

    // Ship
    addShipMoves(state, team, moves);

    // Each pirate
    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = state.pirates[t][i];
        switch (p.state) {
        case PirateState::OnShip:
            addDisembarkMoves(state, p, moves);
            break;
        case PirateState::OnBoard:
            if (p.drunkTurnsLeft > 0) {
                Move m; m.type = MoveType::SkipDrunk; m.pirateId = p.id;
                m.from = p.pos; m.to = p.pos;
                moves.push_back(m);
            } else if (state.isOnActiveSpinner(p)) {
                addSpinnerMoves(state, p, moves);
                // On spinner: can also leave if spinner is done
                // (isOnActiveSpinner returns false when done, so normal moves apply)
            } else if (isWater(p.pos)) {
                addSwimmingMoves(state, p, moves);
            } else {
                addLandMoves(state, p, moves);
            }
            break;
        case PirateState::InTrap:
            // Can't move, waiting for rescue
            break;
        case PirateState::InCave:
            // Stuck in cave
            break;
        case PirateState::Dead:
            break;
        }
    }

    // Character moves (Ben Gunn, Missionary, Friday)
    for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
        auto& ch = state.characters[ci];
        if (ch.owner != team || !ch.discovered || !ch.alive) continue;

        // Character on ship → can disembark (like pirate)
        if (ch.onShip) {
            Coord front = state.shipFront(team);
            if (front.valid() && isLand(front)) {
                Move m;
                m.type = MoveType::MoveCharacter;
                m.pirateId = {team, 100 + ci};
                m.characterIndex = ci;
                m.from = state.ships[static_cast<int>(team)].pos;
                m.to = front;
                moves.push_back(m);
            }
            continue;
        }

        if (!ch.pos.valid() || !isLand(ch.pos)) continue;
        bool carrying = ch.carryingCoin || ch.carryingGalleon;

        for (int d = 0; d < 8; d++) {
            Coord to = ch.pos.moved(static_cast<Direction>(d));
            if (!to.inBounds() || isCorner(to)) continue;
            if (isWater(to)) {
                // Board ally ship
                if (state.isAllyShip(to, team)) {
                    Move m;
                    m.type = MoveType::MoveCharacter;
                    m.pirateId = {team, 100 + ci};
                    m.characterIndex = ci;
                    m.from = ch.pos;
                    m.to = to;
                    moves.push_back(m);
                }
                continue;
            }
            // Carrying coin → only revealed tiles
            if (carrying && !state.tileAt(to).revealed) continue;
            // Missionary can't enter tiles with enemies
            if (ch.type == CharacterType::Missionary) {
                if (state.tileAt(to).revealed && state.hasEnemyAt(to, team)) continue;
            }

            Move m;
            m.type = MoveType::MoveCharacter;
            m.pirateId = {team, 100 + ci};
            m.characterIndex = ci;
            m.from = ch.pos;
            m.to = to;
            moves.push_back(m);
        }
    }

    // Resurrection
    addResurrectionMoves(state, team, moves);

    return moves;
}

// ============================================================
// CHAIN RESOLUTION (arrows, ice, cannon)
// ============================================================

// Forward declarations
static void landOnTile(GameState& s, Pirate& p, Coord pos, Coord from, EventList& events);

static void resolveChain(GameState& s, Pirate& p, Coord from, Coord to, EventList& events) {
    std::set<Coord> visited;
    Coord current = from;
    Coord target = to;
    int maxIter = 200;

    while (maxIter-- > 0) {
        // Cycle detection
        if (visited.count(target)) {
            events.push_back({EventType::PirateDied, target, {}, p.id, p.id.team});
            p.state = PirateState::Dead;
            p.pos = {-1, -1};
            p.carryingCoin = false;
            p.carryingGalleon = false;
            return;
        }
        visited.insert(target);

        // Off the board
        if (!target.inBounds() || isCorner(target)) {
            // Flew off — treat as water
            events.push_back({EventType::PirateMoved, current, target});
            p.state = PirateState::Dead;
            p.pos = {-1, -1};
            return;
        }

        // Water tile
        if (isWater(target)) {
            events.push_back({EventType::PirateMoved, current, target, p.id});
            p.pos = target;
            p.state = PirateState::OnBoard;

            // Enemy ship → die
            if (s.isEnemyShip(target, p.id.team)) {
                events.push_back({EventType::PirateDied, target, {}, p.id, p.id.team});
                p.state = PirateState::Dead;
                p.pos = {-1, -1};
                return;
            }
            // Own/ally ship → board
            if (s.isAllyShip(target, p.id.team)) {
                p.state = PirateState::OnShip;
                if (p.carryingCoin) {
                    int si = s.shipIndexAt(target);
                    if (si >= 0) s.scores[si]++;
                    p.carryingCoin = false;
                    events.push_back({EventType::CoinLoaded, target, {}, p.id});
                }
                if (p.carryingGalleon) {
                    int si = s.shipIndexAt(target);
                    if (si >= 0) s.scores[si] += 3;
                    p.carryingGalleon = false;
                    events.push_back({EventType::CoinLoaded, target, {}, p.id, {}, {}, 3});
                }
                events.push_back({EventType::PirateBoarded, target, {}, p.id});
                return;
            }
            // Coin lost in water
            if (p.carryingCoin) {
                p.carryingCoin = false;
                events.push_back({EventType::CoinLost, target, {}, p.id});
            }
            if (p.carryingGalleon) {
                p.carryingGalleon = false;
                events.push_back({EventType::CoinLost, target, {}, p.id, {}, {}, 3});
            }
            return;
        }

        // Land tile
        Tile& tile = s.tileAt(target);

        // Reveal if closed
        if (!tile.revealed) {
            tile.revealed = true;
            events.push_back({EventType::TileRevealed, target, {}, {}, {}, tile.type});
            // Place coins for treasure
            if (tile.type == TileType::Treasure) {
                tile.coins = tile.treasureValue;
                events.push_back({EventType::CoinPlaced, target, {}, {}, {}, {}, tile.treasureValue});
            }
            if (tile.type == TileType::Galleon) {
                tile.hasGalleonTreasure = true;
                events.push_back({EventType::GalleonPlaced, target});
            }
            if (tile.type == TileType::Rum) {
                events.push_back({EventType::RumFound, target, {}, {}, {}, {}, tile.rumBottles});
            }
        }

        events.push_back({EventType::PirateMoved, current, target, p.id});

        // Apply tile effect
        switch (tile.type) {
        case TileType::Arrow: {
            int ndirs = popcount8(tile.directionBits);
            if (ndirs == 1) {
                // Forced single direction
                for (int d = 0; d < 8; d++) {
                    if (tile.directionBits & dirBit(d)) {
                        current = target;
                        target = target.moved(static_cast<Direction>(d));
                        break;
                    }
                }
                continue; // chain
            } else if (ndirs > 1) {
                // Player must choose
                p.pos = target;
                s.phase = TurnPhase::ChooseArrowDirection;
                s.pendingPirate = p.id;
                s.pendingPos = target;
                s.pendingDirBits = tile.directionBits;
                return;
            }
            // 0 directions (shouldn't happen)
            p.pos = target;
            return;
        }

        case TileType::Ice: {
            // Repeat entry direction
            Direction entryDir = dirFromDelta(target.row - current.row, target.col - current.col);
            if (entryDir == DIR_NONE) { p.pos = target; return; }
            current = target;
            target = target.moved(entryDir);
            continue; // chain
        }

        case TileType::Cannon: {
            // Cannon fires pirate ALL THE WAY to water in the cannon's direction.
            // Gold sinks. Enemy ship = death. Own ship = board.
            Direction fireDir = DIR_NONE;
            for (int d = 0; d < 8; d++) {
                if (tile.directionBits & dirBit(d)) { fireDir = static_cast<Direction>(d); break; }
            }
            events.push_back({EventType::CannonFired, target, {}, p.id});
            if (fireDir == DIR_NONE) { p.pos = target; return; }

            // Fly until reaching water
            Coord flyPos = target;
            while (true) {
                Coord next = flyPos.moved(fireDir);
                if (!next.inBounds() || isCorner(next)) break;
                if (isWater(next)) {
                    flyPos = next;
                    break;
                }
                flyPos = next;
            }

            // Pirate lands in water (or edge of board)
            events.push_back({EventType::PirateMoved, target, flyPos, p.id});
            p.pos = flyPos;

            // Gold sinks
            if (p.carryingCoin) {
                p.carryingCoin = false;
                events.push_back({EventType::CoinLost, flyPos, {}, p.id});
            }
            if (p.carryingGalleon) {
                p.carryingGalleon = false;
                events.push_back({EventType::CoinLost, flyPos, {}, p.id, {}, {}, 3});
            }

            if (isWater(flyPos)) {
                // Check for enemy ship → death
                if (s.isEnemyShip(flyPos, p.id.team)) {
                    events.push_back({EventType::PirateDied, flyPos, {}, p.id, p.id.team});
                    p.state = PirateState::Dead;
                    p.pos = {-1, -1};
                    return;
                }
                // Own/ally ship → board
                if (s.isAllyShip(flyPos, p.id.team)) {
                    p.state = PirateState::OnShip;
                    events.push_back({EventType::PirateBoarded, flyPos, flyPos, p.id});
                    return;
                }
                // Just swimming in water
                p.state = PirateState::OnBoard;
            }
            return;
        }

        case TileType::Horse: {
            // Calculate valid L-destinations
            p.pos = target;
            s.pendingPirate = p.id;
            s.pendingPos = target;
            s.pendingChoices.clear();
            for (int h = 0; h < HORSE_MOVES; h++) {
                Coord dest = {target.row + HORSE_DR[h], target.col + HORSE_DC[h]};
                if (dest.inBounds() && !isCorner(dest)) {
                    s.pendingChoices.push_back(dest);
                }
            }
            if (s.pendingChoices.size() == 1) {
                // Only one option — auto-select
                current = target;
                target = s.pendingChoices[0];
                s.pendingChoices.clear();
                continue; // chain
            }
            s.phase = TurnPhase::ChooseHorseDest;
            return;
        }

        case TileType::Crocodile: {
            // Return to previous tile
            events.push_back({EventType::PirateMoved, target, current, p.id});
            if (isWater(current) || !current.inBounds() || isCorner(current)) {
                // Previous tile is water (e.g. disembarked and hit croc) — board ship
                if (s.isAllyShip(current, p.id.team)) {
                    p.pos = current;
                    p.state = PirateState::OnShip;
                    if (p.carryingCoin) {
                        int si = s.shipIndexAt(current);
                        if (si >= 0) s.scores[si]++;
                        p.carryingCoin = false;
                        events.push_back({EventType::CoinLoaded, current, {}, p.id});
                    }
                    events.push_back({EventType::PirateBoarded, target, current, p.id});
                } else {
                    // Water without own ship — pirate swims
                    p.pos = current;
                    p.state = PirateState::OnBoard;
                }
            } else {
                p.pos = current;
            }
            return;
        }

        case TileType::Cannibal: {
            events.push_back({EventType::PirateDied, target, {}, p.id, p.id.team});
            p.state = PirateState::Dead;
            p.pos = {-1, -1};
            p.carryingCoin = false;
            p.carryingGalleon = false;
            return;
        }

        case TileType::Trap: {
            p.pos = target;
            // Check if there's a friendly/allied pirate already here (rescue!)
            bool hasRescuer = false;
            for (int t = 0; t < s.config.numTeams; t++) {
                if (!isAlly(p.id.team, static_cast<Team>(t), s.config)) continue;
                for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                    auto& fp = s.pirates[t][i];
                    if (fp.id != p.id && fp.pos == target && fp.state == PirateState::OnBoard) {
                        hasRescuer = true;
                    }
                }
            }
            // Free all allied trapped pirates here
            for (int t = 0; t < s.config.numTeams; t++) {
                if (!isAlly(p.id.team, static_cast<Team>(t), s.config)) continue;
                for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                    auto& tp = s.pirates[t][i];
                    if (tp.id != p.id && tp.state == PirateState::InTrap && tp.pos == target) {
                        tp.state = PirateState::OnBoard;
                        events.push_back({EventType::PirateFreed, target, {}, tp.id});
                        hasRescuer = true; // arriving pirate also doesn't get trapped
                    }
                }
            }
            // New pirate on trap: if alone, gets trapped
            if (!hasRescuer) {
                p.state = PirateState::InTrap;
                events.push_back({EventType::PirateTrapped, target, {}, p.id});
            }
            return;
        }

        case TileType::Balloon: {
            Coord shipPos = s.ships[static_cast<int>(p.id.team)].pos;
            events.push_back({EventType::BalloonLiftoff, target, shipPos, p.id});
            p.pos = shipPos;
            p.state = PirateState::OnShip;
            if (p.carryingCoin) {
                s.scores[static_cast<int>(p.id.team)]++;
                p.carryingCoin = false;
                events.push_back({EventType::CoinLoaded, shipPos, {}, p.id});
            }
            if (p.carryingGalleon) {
                s.scores[static_cast<int>(p.id.team)] += 3;
                p.carryingGalleon = false;
                events.push_back({EventType::CoinLoaded, shipPos, {}, p.id, {}, {}, 3});
            }
            return;
        }

        case TileType::Cave: {
            // Find other open caves
            p.pos = target;
            std::vector<Coord> exits;
            for (int r = 1; r <= 11; r++) {
                for (int c = 1; c <= 11; c++) {
                    Coord cc = {r, c};
                    if (cc == target) continue;
                    if (!isLand(cc)) continue;
                    auto& ct = s.tileAt(cc);
                    if (ct.type == TileType::Cave && ct.revealed) {
                        exits.push_back(cc);
                    }
                }
            }
            if (exits.empty()) {
                // No exit — pirate stuck
                p.state = PirateState::InCave;
                events.push_back({EventType::CaveEntered, target, {}, p.id});
            } else if (exits.size() == 1) {
                // One exit — go there
                events.push_back({EventType::CaveExited, target, exits[0], p.id});
                p.pos = exits[0];
                // Landing on exit tile — apply effects there
                landOnTile(s, p, exits[0], target, events);
            } else {
                // Multiple exits — player chooses
                s.phase = TurnPhase::ChooseCaveExit;
                s.pendingPirate = p.id;
                s.pendingPos = target;
                s.pendingChoices = exits;
                events.push_back({EventType::CaveEntered, target, {}, p.id});
            }
            return;
        }

        case TileType::RumBarrel: {
            p.pos = target;
            p.drunkTurnsLeft = 1; // skip next turn
            events.push_back({EventType::PirateDrunk, target, {}, p.id});
            return;
        }

        case TileType::Rum: {
            p.pos = target;
            if (tile.rumBottles > 0) {
                s.rumOwned[static_cast<int>(p.id.team)] += tile.rumBottles;
                events.push_back({EventType::RumFound, target, {}, p.id, {}, {}, tile.rumBottles});
                tile.rumBottles = 0;
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Airplane: {
            p.pos = target;
            if (!tile.used) {
                // Airplane: player can use it now or save for later
                // For simplicity: auto-use on first visit, player chooses destination
                // Actually rules say: "Не хотите лететь прямо сейчас — оставайтесь на клетке"
                // So the pirate stays and can use it on a future turn
                // For now: just land, the airplane is available as a future action
                // TODO: implement airplane as a special action from this tile
                tile.used = true; // mark as used immediately for MVP
                events.push_back({EventType::AirplaneUsed, target, {}, p.id});
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Lighthouse: {
            p.pos = target;
            if (!tile.used) {
                tile.used = true;
                // Reveal 4 random closed tiles for now (proper: player chooses)
                int revealed = 0;
                for (int r = 1; r <= 11 && revealed < 4; r++) {
                    for (int c = 1; c <= 11 && revealed < 4; c++) {
                        if (!isLand({r, c})) continue;
                        auto& lt = s.tileAt({r, c});
                        if (!lt.revealed) {
                            lt.revealed = true;
                            events.push_back({EventType::LighthouseUsed, {r, c}, {}, {}, {}, lt.type});
                            if (lt.type == TileType::Treasure) {
                                lt.coins = lt.treasureValue;
                            }
                            revealed++;
                        }
                    }
                }
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Earthquake: {
            p.pos = target;
            // Swap 2 random empty tiles (proper: player chooses)
            // For MVP: auto-swap first 2 found empty unrevealed tiles
            std::vector<Coord> emptyTiles;
            for (int r = 1; r <= 11; r++) {
                for (int c = 1; c <= 11; c++) {
                    Coord cc = {r, c};
                    if (!isLand(cc)) continue;
                    auto& et = s.tileAt(cc);
                    if (!et.revealed && s.piratesAt(cc).empty()) {
                        emptyTiles.push_back(cc);
                    }
                }
            }
            if (emptyTiles.size() >= 2) {
                std::swap(s.tileAt(emptyTiles[0]), s.tileAt(emptyTiles[1]));
                events.push_back({EventType::EarthquakeTriggered, emptyTiles[0], emptyTiles[1]});
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::BenGunn: {
            p.pos = target;
            // Find an undiscovered BenGunn character
            for (int ci = 0; ci < 3; ci++) {
                if (!s.characters[ci].discovered) {
                    s.characters[ci].discovered = true;
                    s.characters[ci].owner = p.id.team;
                    s.characters[ci].pos = target;
                    events.push_back({EventType::CharacterJoined, target, {}, p.id, p.id.team,
                                      {}, ci});
                    break;
                }
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Missionary: {
            p.pos = target;
            if (!s.characters[3].discovered) {
                s.characters[3].discovered = true;
                s.characters[3].owner = p.id.team;
                s.characters[3].pos = target;
                events.push_back({EventType::CharacterJoined, target, {}, p.id, p.id.team, {}, 3});
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Friday: {
            p.pos = target;
            if (!s.characters[4].discovered) {
                s.characters[4].discovered = true;
                s.characters[4].owner = p.id.team;
                s.characters[4].pos = target;
                events.push_back({EventType::CharacterJoined, target, {}, p.id, p.id.team, {}, 4});
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Grass: {
            p.pos = target;
            // Swap turn order with next player
            if (s.numActivePlayers > 1) {
                int nextIdx = (s.currentPlayerIndex + 1) % s.numActivePlayers;
                std::swap(s.turnOrder[s.currentPlayerIndex], s.turnOrder[nextIdx]);
                events.push_back({EventType::TurnOrderChanged, target});
            }
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::Caramba: {
            p.pos = target;
            // Random bad event — for MVP just a cosmetic effect
            events.push_back({EventType::PirateMoved, target, target, p.id});
            landOnTile(s, p, target, current, events);
            return;
        }

        case TileType::ThickJungle: {
            // No combat here, no items. Multiple teams can coexist.
            p.pos = target;
            return;
        }

        default: // Empty, Fortress, ResurrectFort, Treasure, Galleon, Spinner
            p.pos = target;
            landOnTile(s, p, target, current, events);
            return;
        }
    }

    // Shouldn't reach here, but if loop exhausted = cycle
    events.push_back({EventType::PirateDied, p.pos, {}, p.id});
    p.state = PirateState::Dead;
    p.pos = {-1, -1};
}

// ============================================================
// LANDING EFFECTS (combat, coin pickup, spinner entry)
// ============================================================

static void landOnTile(GameState& s, Pirate& p, Coord pos, Coord from, EventList& events) {
    Tile& tile = s.tileAt(pos);
    Team team = p.id.team;

    // Spinner entry
    if (isSpinner(tile.type)) {
        p.spinnerProgress = 1;
        events.push_back({EventType::SpinnerAdvanced, pos, {}, p.id, {}, {}, 1});
        // Check combat on spinner (can attack if 1 step behind)
        auto enemies = s.enemyPiratesAt(pos, team);
        for (auto& eid : enemies) {
            auto& enemy = s.pirateRef(eid);
            if (enemy.spinnerProgress == p.spinnerProgress) {
                // Same step — attack
                events.push_back({EventType::PirateAttacked, pos, {}, eid, eid.team});
                if (enemy.carryingCoin) {
                    tile.coins++;
                    enemy.carryingCoin = false;
                    events.push_back({EventType::CoinDropped, pos, {}, eid});
                }
                enemy.pos = s.ships[static_cast<int>(eid.team)].pos;
                enemy.state = PirateState::OnShip;
                enemy.spinnerProgress = 0;
            }
        }
        return;
    }

    // Combat on normal tiles (not ThickJungle, not Fortress)
    if (tile.type != TileType::ThickJungle && tile.type != TileType::Fortress) {
        auto enemies = s.enemyPiratesAt(pos, team);
        if (!enemies.empty()) {
            // Can only attack with empty hands
            // If carrying coin: drop it first
            if (p.carryingCoin) {
                // Drop coin at FROM position
                s.tileAt(from).coins++;
                p.carryingCoin = false;
                events.push_back({EventType::CoinDropped, from, {}, p.id});
            }
            if (p.carryingGalleon) {
                s.tileAt(from).hasGalleonTreasure = true;
                p.carryingGalleon = false;
                events.push_back({EventType::CoinDropped, from, {}, p.id, {}, {}, 3});
            }

            // Send all enemies back to their ships
            for (auto& eid : enemies) {
                auto& enemy = s.pirateRef(eid);
                events.push_back({EventType::PirateAttacked, pos, {}, eid, eid.team});
                if (enemy.carryingCoin) {
                    tile.coins++;
                    enemy.carryingCoin = false;
                    events.push_back({EventType::CoinDropped, pos, {}, eid});
                }
                if (enemy.carryingGalleon) {
                    tile.hasGalleonTreasure = true;
                    enemy.carryingGalleon = false;
                    events.push_back({EventType::CoinDropped, pos, {}, eid, {}, {}, 3});
                }
                enemy.pos = s.ships[static_cast<int>(eid.team)].pos;
                enemy.state = PirateState::OnShip;
                enemy.spinnerProgress = 0;
            }
        }
    }

    // Auto-pickup: pirate takes 1 coin when landing (part of the same turn, free action).
    // Per board game rules: you pick up a coin upon entering the tile.
    if (!p.carryingCoin && !p.carryingGalleon) {
        if (tile.hasGalleonTreasure) {
            tile.hasGalleonTreasure = false;
            p.carryingGalleon = true;
            events.push_back({EventType::CoinPickedUp, pos, {}, p.id, {}, {}, 3});
        } else if (tile.coins > 0) {
            tile.coins--;
            p.carryingCoin = true;
            events.push_back({EventType::CoinPickedUp, pos, {}, p.id, {}, {}, 1});
        }
    }

    // Free trapped pirates on this tile
    int ti = static_cast<int>(team);
    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& tp = s.pirates[ti][i];
        if (tp.id != p.id && tp.state == PirateState::InTrap && tp.pos == pos) {
            tp.state = PirateState::OnBoard;
            events.push_back({EventType::PirateFreed, pos, {}, tp.id});
        }
    }

    // Free pirates stuck in caves when this cave is revealed
    if (tile.type == TileType::Cave) {
        for (int t = 0; t < s.config.numTeams; t++) {
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& cp = s.pirates[t][i];
                if (cp.state == PirateState::InCave) {
                    cp.state = PirateState::OnBoard;
                    cp.pos = pos;
                    events.push_back({EventType::CaveExited, {-1,-1}, pos, cp.id});
                }
            }
        }
    }
}

// ============================================================
// APPLY MOVE
// ============================================================

EventList applyMove(GameState& state, const Move& move) {
    EventList events;

    switch (move.type) {
    case MoveType::MoveShip: {
        int si = static_cast<int>(move.pirateId.team);
        if (si < 0) {
            // Find which team's ship
            si = state.shipIndexAt(move.from);
        }
        if (si >= 0) {
            state.ships[si].pos = move.to;
            // Move all pirates on ship
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = state.pirates[si][i];
                if (p.state == PirateState::OnShip) p.pos = move.to;
            }
            events.push_back({EventType::ShipMoved, move.from, move.to, {},
                              static_cast<Team>(si)});
        }
        state.advanceTurn();
        break;
    }

    case MoveType::DisembarkPirate: {
        auto& p = state.pirateRef(move.pirateId);
        p.state = PirateState::OnBoard;
        events.push_back({EventType::PirateDisembarked, move.from, move.to, p.id});
        resolveChain(state, p, move.from, move.to, events);
        if (state.phase == TurnPhase::ChooseAction)
            state.advanceTurn();
        break;
    }

    case MoveType::MovePirate: {
        auto& p = state.pirateRef(move.pirateId);
        Coord from = p.pos;
        p.spinnerProgress = 0; // Reset when leaving any tile
        if (isWater(move.to) && !isLand(move.to)) {
            // Swimming move
            p.pos = move.to;
            events.push_back({EventType::PirateMoved, from, move.to, p.id});
            state.advanceTurn();
        } else {
            resolveChain(state, p, from, move.to, events);
            if (state.phase == TurnPhase::ChooseAction)
                state.advanceTurn();
        }
        break;
    }

    case MoveType::BoardShip: {
        auto& p = state.pirateRef(move.pirateId);
        Coord from = p.pos;
        p.state = PirateState::OnShip;
        p.pos = move.to;
        p.spinnerProgress = 0;
        if (p.carryingCoin) {
            int si = state.shipIndexAt(move.to);
            if (si >= 0) state.scores[si]++;
            p.carryingCoin = false;
            events.push_back({EventType::CoinLoaded, move.to, {}, p.id});
        }
        if (p.carryingGalleon) {
            int si = state.shipIndexAt(move.to);
            if (si >= 0) state.scores[si] += 3;
            p.carryingGalleon = false;
            events.push_back({EventType::CoinLoaded, move.to, {}, p.id, {}, {}, 3});
        }
        events.push_back({EventType::PirateBoarded, from, move.to, p.id});
        state.advanceTurn();
        break;
    }

    case MoveType::AdvanceSpinner: {
        auto& p = state.pirateRef(move.pirateId);
        p.spinnerProgress++;
        int required = spinnerSteps(state.tileAt(p.pos).type);
        events.push_back({EventType::SpinnerAdvanced, p.pos, {}, p.id, {},
                          {}, p.spinnerProgress});

        // Combat: attack enemy at same progress level
        auto enemies = state.enemyPiratesAt(p.pos, p.id.team);
        for (auto& eid : enemies) {
            auto& enemy = state.pirateRef(eid);
            if (enemy.spinnerProgress == p.spinnerProgress) {
                events.push_back({EventType::PirateAttacked, p.pos, {}, eid, eid.team});
                if (enemy.carryingCoin) {
                    state.tileAt(p.pos).coins++;
                    enemy.carryingCoin = false;
                }
                enemy.pos = state.ships[static_cast<int>(eid.team)].pos;
                enemy.state = PirateState::OnShip;
                enemy.spinnerProgress = 0;
            }
        }

        // NO auto-pickup after spinner. Player uses PickupCoin explicitly.

        state.advanceTurn();
        break;
    }

    case MoveType::ResurrectPirate: {
        auto& p = state.pirateRef(move.pirateId);
        p.state = PirateState::OnBoard;
        p.pos = move.to; // resurrection fort position
        p.carryingCoin = false;
        p.carryingGalleon = false;
        p.spinnerProgress = 0;
        p.drunkTurnsLeft = 0;
        events.push_back({EventType::PirateResurrected, move.to, {}, p.id, p.id.team});
        state.advanceTurn();
        break;
    }

    case MoveType::ChooseDirection: {
        auto& p = state.pirateRef(move.pirateId);
        state.phase = TurnPhase::ChooseAction;
        state.pendingDirBits = 0;
        Coord from = p.pos;
        Coord to = from.moved(move.chosenDir);
        resolveChain(state, p, from, to, events);
        if (state.phase == TurnPhase::ChooseAction)
            state.advanceTurn();
        break;
    }

    case MoveType::ChooseHorseDest: {
        auto& p = state.pirateRef(move.pirateId);
        state.phase = TurnPhase::ChooseAction;
        state.pendingChoices.clear();
        Coord from = p.pos;
        resolveChain(state, p, from, move.to, events);
        if (state.phase == TurnPhase::ChooseAction)
            state.advanceTurn();
        break;
    }

    case MoveType::ChooseCaveExit: {
        auto& p = state.pirateRef(move.pirateId);
        state.phase = TurnPhase::ChooseAction;
        state.pendingChoices.clear();
        p.state = PirateState::OnBoard;
        p.pos = move.to;
        events.push_back({EventType::CaveExited, move.from, move.to, p.id});
        landOnTile(state, p, move.to, move.from, events);
        if (state.phase == TurnPhase::ChooseAction)
            state.advanceTurn();
        break;
    }

    case MoveType::MoveCharacter: {
        int ci = move.characterIndex;
        if (ci < 0 || ci >= MAX_CHARACTERS) { state.advanceTurn(); break; }
        auto& ch = state.characters[ci];
        Coord from = ch.onShip ? state.ships[static_cast<int>(ch.owner)].pos : ch.pos;

        // Disembarking from ship
        if (ch.onShip) {
            ch.onShip = false;
            ch.pos = move.to;
            events.push_back({EventType::CharacterMoved, from, move.to, {}, ch.owner, {}, ci});
            // Reveal tile
            if (isLand(move.to)) {
                auto& tile = state.tileAt(move.to);
                if (!tile.revealed) {
                    tile.revealed = true;
                    events.push_back({EventType::TileRevealed, move.to, {}, {}, {}, tile.type});
                    if (tile.type == TileType::Treasure) {
                        tile.coins = tile.treasureValue;
                    }
                    if (tile.type == TileType::Galleon) tile.hasGalleonTreasure = true;
                }
                // Auto-pickup coin
                if (!ch.carryingCoin && !ch.carryingGalleon) {
                    auto& t2 = state.tileAt(move.to);
                    if (t2.hasGalleonTreasure) { t2.hasGalleonTreasure = false; ch.carryingGalleon = true; }
                    else if (t2.coins > 0) { t2.coins--; ch.carryingCoin = true; }
                }
            }
            state.advanceTurn();
            break;
        }

        // Boarding ship — load coins
        if (isWater(move.to) && state.isAllyShip(move.to, ch.owner)) {
            ch.onShip = true;
            ch.pos = move.to;
            int si = state.shipIndexAt(move.to);
            if (ch.carryingCoin && si >= 0) {
                state.scores[si]++;
                ch.carryingCoin = false;
                events.push_back({EventType::CoinLoaded, move.to, {}, {}, ch.owner, {}, 1});
            }
            if (ch.carryingGalleon && si >= 0) {
                state.scores[si] += 3;
                ch.carryingGalleon = false;
                events.push_back({EventType::CoinLoaded, move.to, {}, {}, ch.owner, {}, 3});
            }
            events.push_back({EventType::CharacterMoved, from, move.to, {}, ch.owner, {}, ci});
            state.advanceTurn();
            break;
        }

        ch.pos = move.to;

        // Reveal tile
        if (isLand(move.to)) {
            auto& tile = state.tileAt(move.to);
            if (!tile.revealed) {
                tile.revealed = true;
                events.push_back({EventType::TileRevealed, move.to, {}, {}, {}, tile.type});
                if (tile.type == TileType::Treasure) {
                    tile.coins = tile.treasureValue;
                    events.push_back({EventType::CoinPlaced, move.to, {}, {}, {}, {}, tile.treasureValue});
                }
                if (tile.type == TileType::Galleon) tile.hasGalleonTreasure = true;
                if (tile.type == TileType::Rum) {
                    state.rumOwned[static_cast<int>(ch.owner)] += tile.rumBottles;
                    tile.rumBottles = 0;
                }
            }

            // Ben Gunn can attack
            if (ch.type == CharacterType::BenGunn) {
                auto enemies = state.enemyPiratesAt(move.to, ch.owner);
                for (auto& eid : enemies) {
                    auto& enemy = state.pirateRef(eid);
                    events.push_back({EventType::PirateAttacked, move.to, {}, eid, eid.team});
                    if (enemy.carryingCoin) { state.tileAt(move.to).coins++; enemy.carryingCoin = false; }
                    if (enemy.carryingGalleon) { state.tileAt(move.to).hasGalleonTreasure = true; enemy.carryingGalleon = false; }
                    enemy.pos = state.ships[static_cast<int>(eid.team)].pos;
                    enemy.state = PirateState::OnShip;
                    enemy.spinnerProgress = 0;
                }
            }

            // Dangerous tiles
            if (tile.type == TileType::Cannibal && ch.type != CharacterType::Friday) {
                ch.alive = false; ch.pos = {-1, -1};
                events.push_back({EventType::CharacterDied, move.to, {}, {}, ch.owner, {}, ci});
                state.advanceTurn(); break;
            }
            if (tile.type == TileType::Balloon) {
                Coord shipPos = state.ships[static_cast<int>(ch.owner)].pos;
                ch.pos = shipPos; ch.onShip = true;
                if (ch.carryingCoin) {
                    int si = state.shipIndexAt(shipPos);
                    if (si >= 0) state.scores[si]++;
                    ch.carryingCoin = false;
                    events.push_back({EventType::CoinLoaded, shipPos, {}, {}, ch.owner, {}, 1});
                }
                if (ch.carryingGalleon) {
                    int si = state.shipIndexAt(shipPos);
                    if (si >= 0) state.scores[si] += 3;
                    ch.carryingGalleon = false;
                    events.push_back({EventType::CoinLoaded, shipPos, {}, {}, ch.owner, {}, 3});
                }
                events.push_back({EventType::BalloonLiftoff, move.to, shipPos, {}, ch.owner, {}, ci});
                state.advanceTurn(); break;
            }

            // Auto-pickup coin
            if (!ch.carryingCoin && !ch.carryingGalleon) {
                auto& t2 = state.tileAt(move.to);
                if (t2.hasGalleonTreasure) {
                    t2.hasGalleonTreasure = false; ch.carryingGalleon = true;
                    events.push_back({EventType::CoinPickedUp, move.to, {}, {}, ch.owner, {}, 3});
                } else if (t2.coins > 0) {
                    t2.coins--; ch.carryingCoin = true;
                    events.push_back({EventType::CoinPickedUp, move.to, {}, {}, ch.owner, {}, 1});
                }
            }
        }

        events.push_back({EventType::CharacterMoved, from, move.to, {}, ch.owner, {}, ci});
        state.advanceTurn();
        break;
    }

    case MoveType::PickupCoin: {
        auto& p = state.pirateRef(move.pirateId);
        auto& tile = state.tileAt(p.pos);
        if (!p.carryingCoin && !p.carryingGalleon) {
            if (tile.hasGalleonTreasure) {
                tile.hasGalleonTreasure = false;
                p.carryingGalleon = true;
                events.push_back({EventType::CoinPickedUp, p.pos, {}, p.id, {}, {}, 3});
            } else if (tile.coins > 0) {
                tile.coins--;
                p.carryingCoin = true;
                events.push_back({EventType::CoinPickedUp, p.pos, {}, p.id, {}, {}, 1});
            }
        }
        state.advanceTurn();
        break;
    }

    case MoveType::SkipDrunk: {
        state.advanceTurn();
        break;
    }

    default:
        state.advanceTurn();
        break;
    }

    return events;
}

// ============================================================
// GAME OVER
// ============================================================

int coinsRemaining(const GameState& state) {
    int total = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            auto& tile = state.board[r][c];
            total += tile.coins;
            if (tile.hasGalleonTreasure) total += 3;
            // Count UNREVEALED treasure — coins not yet placed but still in play
            if (!tile.revealed) {
                if (tile.type == TileType::Treasure)
                    total += tile.treasureValue;
                if (tile.type == TileType::Galleon)
                    total += 3;
            }
        }
    // Coins carried by pirates
    for (int t = 0; t < state.config.numTeams; t++)
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            if (state.pirates[t][i].carryingCoin) total++;
            if (state.pirates[t][i].carryingGalleon) total += 3;
        }
    return total;
}

bool isGameOver(const GameState& state) {
    // Game over when no coins remain on the board or carried
    if (coinsRemaining(state) == 0) return true;

    // Or if only one team can still play
    int activeTeams = 0;
    for (int t = 0; t < state.config.numTeams; t++) {
        if (state.isPlayerActive(static_cast<Team>(t))) activeTeams++;
    }
    return activeTeams <= 1;
}

Team getWinner(const GameState& state) {
    int maxScore = -1;
    Team winner = Team::None;
    for (int t = 0; t < state.config.numTeams; t++) {
        int score = state.scores[t];
        // In team mode, combine allied scores
        if (state.config.teamMode && state.config.numTeams == 4) {
            int ally = (t + 2) % 4;
            score += state.scores[ally];
        }
        if (score > maxScore) {
            maxScore = score;
            winner = static_cast<Team>(t);
        }
    }
    return winner;
}

} // namespace Rules
