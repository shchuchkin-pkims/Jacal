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

// Get ship side direction (toward nearest land)
static Direction shipInwardDir(const GameState& s, Coord shipPos) {
    for (int d = 0; d < 8; d++) {
        Coord adj = shipPos.moved(static_cast<Direction>(d));
        if (adj.inBounds() && s.mapIsLand(adj))
            return static_cast<Direction>(d);
    }
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
    case TurnPhase::ChooseAirplaneTarget: {
        for (auto& dest : s.pendingChoices) {
            Move m;
            m.type = MoveType::ChooseAirplaneDest;
            m.pirateId = s.pendingPirate;
            m.from = s.pendingPos;
            m.to = dest;
            moves.push_back(m);
        }
        break;
    }
    case TurnPhase::ChooseLighthouseTiles: {
        for (auto& dest : s.pendingChoices) {
            Move m;
            m.type = MoveType::ChooseLighthouseTiles;
            m.pirateId = s.pendingPirate;
            m.from = s.pendingPos;
            m.to = dest;
            moves.push_back(m);
        }
        break;
    }
    case TurnPhase::ChooseEarthquakeTiles: {
        for (auto& dest : s.pendingChoices) {
            Move m;
            m.type = MoveType::ChooseEarthquakeTiles;
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

    // Ship can move to any adjacent WATER cell that is next to at least one LAND cell
    // (i.e. stays along the coastline)
    for (int d = 0; d < 8; d++) {
        Coord dest = pos.moved(static_cast<Direction>(d));
        if (!dest.inBounds()) continue;
        if (!s.mapIsWater(dest)) continue;        // must be water
        if (s.mapIsRock(dest)) continue;           // can't go on rocks
        if (s.shipIndexAt(dest) >= 0) continue;    // can't stack ships

        // Must be adjacent to at least one land cell (coastline)
        bool nearLand = false;
        for (int d2 = 0; d2 < 8; d2++) {
            Coord adj = dest.moved(static_cast<Direction>(d2));
            if (adj.inBounds() && s.mapIsLand(adj)) { nearLand = true; break; }
        }
        if (!nearLand) continue;

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
        if (!to.inBounds() || s.mapIsRock(to)) continue;

        // Water: normally can't jump in from land
        if (s.mapIsWater(to)) {
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

        // Fortress / ResurrectFort: can't enter if enemy is there
        if (tile.revealed && (tile.type == TileType::Fortress || tile.type == TileType::ResurrectFort)
            && s.hasEnemyAt(to, team)) continue;

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
        if (!to.inBounds() || s.mapIsRock(to)) continue;

        if (s.mapIsWater(to)) {
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
        } else if (s.mapIsLand(to)) {
            // Per rules: pirate in water can ONLY swim along coast to own ship.
            // Cannot exit water onto land directly.
            continue;
        }
    }
}

// ============================================================
// DISEMBARK
// ============================================================

static void addDisembarkMoves(const GameState& s, const Pirate& p, MoveList& moves) {
    Coord front = s.shipFront(p.id.team);
    if (!front.valid() || !s.mapIsLand(front)) return;

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
        if (p.state == PirateState::OnBoard && s.mapIsLand(p.pos)) {
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
            } else if (state.mapIsWater(p.pos)) {
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
            if (front.valid() && state.mapIsLand(front)) {
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

        if (!ch.pos.valid() || !state.mapIsLand(ch.pos)) continue;
        bool carrying = ch.carryingCoin || ch.carryingGalleon;

        for (int d = 0; d < 8; d++) {
            Coord to = ch.pos.moved(static_cast<Direction>(d));
            if (!to.inBounds() || state.mapIsRock(to)) continue;
            if (state.mapIsWater(to)) {
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

    // Use rum (if team has rum bottles)
    if (state.rumOwned[t] > 0) {
        // Free trapped pirates
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            auto& tp = state.pirates[t][i];
            if (tp.state == PirateState::InTrap) {
                Move m; m.type = MoveType::UseRum;
                m.pirateId = tp.id; m.from = tp.pos; m.to = tp.pos;
                moves.push_back(m);
            }
            if (tp.state == PirateState::InCave) {
                Move m; m.type = MoveType::UseRum;
                m.pirateId = tp.id; m.from = tp.pos; m.to = tp.pos;
                moves.push_back(m);
            }
        }
        // Use rum on Friday (any team's) on adjacent tile → Friday disappears
        for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
            auto& ch = state.characters[ci];
            if (!ch.discovered || !ch.alive || ch.type != CharacterType::Friday) continue;
            // Check if any of our pirates is adjacent to Friday
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p2 = state.pirates[t][i];
                if (p2.state != PirateState::OnBoard) continue;
                int dr = std::abs(p2.pos.row - ch.pos.row);
                int dc = std::abs(p2.pos.col - ch.pos.col);
                if (dr <= 1 && dc <= 1 && (dr + dc > 0)) {
                    Move m; m.type = MoveType::UseRum;
                    m.pirateId = p2.id; m.characterIndex = ci;
                    m.from = p2.pos; m.to = ch.pos;
                    moves.push_back(m);
                    break;
                }
            }
        }
        // Use rum on Missionary on adjacent tile → becomes regular pirate
        for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
            auto& ch = state.characters[ci];
            if (!ch.discovered || !ch.alive || ch.type != CharacterType::Missionary) continue;
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p2 = state.pirates[t][i];
                if (p2.state != PirateState::OnBoard) continue;
                int dr = std::abs(p2.pos.row - ch.pos.row);
                int dc = std::abs(p2.pos.col - ch.pos.col);
                if (dr <= 1 && dc <= 1 && (dr + dc > 0)) {
                    Move m; m.type = MoveType::UseRum;
                    m.pirateId = p2.id; m.characterIndex = ci;
                    m.from = p2.pos; m.to = ch.pos;
                    moves.push_back(m);
                    break;
                }
            }
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
        if (!target.inBounds() || s.mapIsRock(target)) {
            // Flew off — treat as water
            events.push_back({EventType::PirateMoved, current, target});
            p.state = PirateState::Dead;
            p.pos = {-1, -1};
            return;
        }

        // Water tile
        if (s.mapIsWater(target)) {
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
            // Cannon fires pirate to sea in cannon's cardinal direction.
            // Own ship = board (gold scores!). Enemy ship = death. Otherwise gold sinks.
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
                if (!next.inBounds() || s.mapIsRock(next)) break;
                if (s.mapIsWater(next)) {
                    flyPos = next;
                    break;
                }
                flyPos = next;
            }

            events.push_back({EventType::PirateMoved, target, flyPos, p.id});
            p.pos = flyPos;

            if (s.mapIsWater(flyPos)) {
                // Check for own/ally ship FIRST — gold scores!
                if (s.isAllyShip(flyPos, p.id.team)) {
                    p.state = PirateState::OnShip;
                    if (p.carryingCoin) {
                        int si = s.shipIndexAt(flyPos);
                        if (si >= 0) s.scores[si]++;
                        p.carryingCoin = false;
                        events.push_back({EventType::CoinLoaded, flyPos, {}, p.id});
                    }
                    if (p.carryingGalleon) {
                        int si = s.shipIndexAt(flyPos);
                        if (si >= 0) s.scores[si] += 3;
                        p.carryingGalleon = false;
                        events.push_back({EventType::CoinLoaded, flyPos, {}, p.id, {}, {}, 3});
                    }
                    events.push_back({EventType::PirateBoarded, flyPos, flyPos, p.id});
                    return;
                }
                // Enemy ship → death
                if (s.isEnemyShip(flyPos, p.id.team)) {
                    // Gold sinks before death
                    if (p.carryingCoin) { p.carryingCoin = false; events.push_back({EventType::CoinLost, flyPos, {}, p.id}); }
                    if (p.carryingGalleon) { p.carryingGalleon = false; events.push_back({EventType::CoinLost, flyPos, {}, p.id, {}, {}, 3}); }
                    events.push_back({EventType::PirateDied, flyPos, {}, p.id, p.id.team});
                    p.state = PirateState::Dead;
                    p.pos = {-1, -1};
                    return;
                }
                // Just swimming in water — gold sinks
                if (p.carryingCoin) { p.carryingCoin = false; events.push_back({EventType::CoinLost, flyPos, {}, p.id}); }
                if (p.carryingGalleon) { p.carryingGalleon = false; events.push_back({EventType::CoinLost, flyPos, {}, p.id, {}, {}, 3}); }
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
                if (dest.inBounds() && !s.mapIsRock(dest)) {
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
            if (s.mapIsWater(current) || !current.inBounds() || s.mapIsRock(current)) {
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
                    if (!s.mapIsLand(cc)) continue;
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
            p.drunkTurnsLeft = 2; // =2 because advanceTurn decrements at start of turn
            events.push_back({EventType::PirateDrunk, target, {}, p.id});
            landOnTile(s, p, target, current, events); // triggers combat
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
                // Player chooses any land tile to fly to. Phase = ChooseAirplaneTarget.
                // Collect all valid destinations (any land tile).
                s.pendingPirate = p.id;
                s.pendingPos = target;
                s.pendingChoices.clear();
                for (int r = 1; r <= 11; r++)
                    for (int c = 1; c <= 11; c++) {
                        Coord cc = {r, c};
                        if (!s.mapIsLand(cc)) continue;
                        if (cc == target) continue; // not the airplane itself
                        s.pendingChoices.push_back(cc);
                    }
                s.phase = TurnPhase::ChooseAirplaneTarget;
                tile.used = true;
                events.push_back({EventType::AirplaneUsed, target, {}, p.id});
            } else {
                landOnTile(s, p, target, current, events);
            }
            return;
        }

        case TileType::Lighthouse: {
            p.pos = target;
            if (!tile.used) {
                tile.used = true;
                s.pendingPirate = p.id;
                s.pendingPos = target;
                s.pendingChoices.clear();
                s.lighthouseRemaining = 4; // pick 4 tiles
                for (int r = 1; r <= 11; r++)
                    for (int c = 1; c <= 11; c++) {
                        Coord cc = {r, c};
                        if (!s.mapIsLand(cc)) continue;
                        if (!s.tileAt(cc).revealed)
                            s.pendingChoices.push_back(cc);
                    }
                int available = static_cast<int>(s.pendingChoices.size());
                if (available < s.lighthouseRemaining)
                    s.lighthouseRemaining = available;
                if (s.lighthouseRemaining > 0) {
                    s.phase = TurnPhase::ChooseLighthouseTiles;
                    events.push_back({EventType::LighthouseUsed, target, {}, p.id});
                } else {
                    landOnTile(s, p, target, current, events);
                }
            } else {
                landOnTile(s, p, target, current, events);
            }
            return;
        }

        case TileType::Earthquake: {
            p.pos = target;
            // Player picks 2 closed tiles (no pirates) to swap.
            s.pendingPirate = p.id;
            s.pendingPos = target;
            s.pendingChoices.clear();
            for (int r = 1; r <= 11; r++)
                for (int c = 1; c <= 11; c++) {
                    Coord cc = {r, c};
                    if (!s.mapIsLand(cc)) continue;
                    auto& et = s.tileAt(cc);
                    if (!et.revealed && s.piratesAt(cc).empty())
                        s.pendingChoices.push_back(cc);
                }
            if (s.pendingChoices.size() >= 2) {
                s.phase = TurnPhase::ChooseEarthquakeTiles;
                events.push_back({EventType::EarthquakeTriggered, target, {}});
            } else {
                landOnTile(s, p, target, current, events);
            }
            return;
        }

        case TileType::BenGunn: {
            p.pos = target;
            // Only 1 Ben Gunn can exist. Find first undiscovered, but only if none already active.
            bool benGunnExists = false;
            for (int ci = 0; ci < 3; ci++) {
                if (s.characters[ci].type == CharacterType::BenGunn && s.characters[ci].discovered && s.characters[ci].alive)
                    benGunnExists = true;
            }
            if (!benGunnExists) {
                for (int ci = 0; ci < 3; ci++) {
                    if (s.characters[ci].type == CharacterType::BenGunn && !s.characters[ci].discovered) {
                        s.characters[ci].discovered = true;
                        s.characters[ci].owner = p.id.team;
                        s.characters[ci].pos = target;
                        events.push_back({EventType::CharacterJoined, target, {}, p.id, p.id.team, {}, ci});
                        break;
                    }
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
            // Voodoo Grass: for the next round, each player controls
            // the team of their clockwise neighbor.
            // Implementation: shift turnOrder so position i controls team (i+1)
            if (s.numActivePlayers > 1 && s.grassRoundsLeft == 0) {
                s.grassRoundsLeft = s.numActivePlayers; // one full round
                // Shift: player at seat 0 now controls team that was at seat 1, etc.
                // Save first, then rotate
                int first = s.turnOrder[0];
                for (int i = 0; i < s.numActivePlayers - 1; i++)
                    s.turnOrder[i] = s.turnOrder[i + 1];
                s.turnOrder[s.numActivePlayers - 1] = first;
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
    // Missionary prevents combat: if Missionary is on this tile, no attacks
    bool missionaryPresent = false;
    for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
        auto& ch = s.characters[ci];
        if (ch.type == CharacterType::Missionary && ch.discovered && ch.alive && ch.pos == pos)
            missionaryPresent = true;
    }

    if (tile.type != TileType::ThickJungle && tile.type != TileType::Fortress
        && tile.type != TileType::ResurrectFort && !missionaryPresent) {
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

            // Check if Friday is at this position — she switches teams instead of dying
            for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
                auto& fch = s.characters[ci];
                if (fch.type == CharacterType::Friday && fch.discovered && fch.alive && fch.pos == pos) {
                    fch.owner = team; // Friday joins the attacker's team
                    events.push_back({EventType::CharacterJoined, pos, {}, p.id, team, {}, ci});
                }
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

            // Also attack enemy characters (BenGunn acts like pirate)
            for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
                auto& ech = s.characters[ci];
                if (!ech.discovered || !ech.alive || ech.pos != pos) continue;
                if (ech.type == CharacterType::Friday) continue; // Friday handled above
                if (ech.type == CharacterType::Missionary) continue; // Missionary can't be attacked
                if (ech.owner == team) continue; // own character
                if (isAlly(team, ech.owner, s.config)) continue;
                // Enemy Ben Gunn → send to owner's ship
                ech.pos = s.ships[static_cast<int>(ech.owner)].pos;
                events.push_back({EventType::CharacterMoved, pos, ech.pos, {}, ech.owner, {}, ci});
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
        if (state.mapIsWater(move.to) && !state.mapIsLand(move.to)) {
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

    case MoveType::ChooseAirplaneDest: {
        auto& p = state.pirateRef(move.pirateId);
        state.phase = TurnPhase::ChooseAction;
        state.pendingChoices.clear();
        // Fly pirate to chosen destination
        Coord from = p.pos;
        p.pos = move.to;
        events.push_back({EventType::PirateMoved, from, move.to, p.id});
        // Reveal destination if closed
        auto& tile = state.tileAt(move.to);
        if (!tile.revealed) {
            tile.revealed = true;
            events.push_back({EventType::TileRevealed, move.to, {}, {}, {}, tile.type});
            if (tile.type == TileType::Treasure) {
                tile.coins = tile.treasureValue;
                events.push_back({EventType::CoinPlaced, move.to, {}, {}, {}, {}, tile.treasureValue});
            }
            if (tile.type == TileType::Galleon) tile.hasGalleonTreasure = true;
        }
        // Landing effects (combat, coin pickup)
        landOnTile(state, p, move.to, from, events);
        if (state.phase == TurnPhase::ChooseAction)
            state.advanceTurn();
        break;
    }

    case MoveType::ChooseLighthouseTiles: {
        // Reveal the chosen tile
        auto& tile = state.tileAt(move.to);
        if (!tile.revealed) {
            tile.revealed = true;
            events.push_back({EventType::TileRevealed, move.to, {}, {}, {}, tile.type});
            if (tile.type == TileType::Treasure) {
                tile.coins = tile.treasureValue;
                events.push_back({EventType::CoinPlaced, move.to, {}, {}, {}, {}, tile.treasureValue});
            }
            if (tile.type == TileType::Galleon) tile.hasGalleonTreasure = true;
        }
        // Remove from choices
        auto& ch = state.pendingChoices;
        ch.erase(std::remove(ch.begin(), ch.end(), move.to), ch.end());
        state.lighthouseRemaining--;

        if (state.lighthouseRemaining <= 0 || ch.empty()) {
            // Done picking — end turn
            state.phase = TurnPhase::ChooseAction;
            state.pendingChoices.clear();
            state.advanceTurn();
        }
        // else: stay in ChooseLighthouseTiles phase for next pick
        break;
    }

    case MoveType::ChooseEarthquakeTiles: {
        if (!state.earthquakeFirst.valid()) {
            // First pick
            state.earthquakeFirst = move.to;
            // Remove from choices
            auto& ch = state.pendingChoices;
            ch.erase(std::remove(ch.begin(), ch.end(), move.to), ch.end());
            // Stay in phase for second pick
        } else {
            // Second pick — swap the two tiles
            std::swap(state.tileAt(state.earthquakeFirst), state.tileAt(move.to));
            events.push_back({EventType::EarthquakeTriggered, state.earthquakeFirst, move.to});
            state.phase = TurnPhase::ChooseAction;
            state.pendingChoices.clear();
            state.earthquakeFirst = {-1, -1};
            state.advanceTurn();
        }
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
            if (state.mapIsLand(move.to)) {
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
        if (state.mapIsWater(move.to) && state.isAllyShip(move.to, ch.owner)) {
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
        if (state.mapIsLand(move.to)) {
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

            // Friday special: ignores traps, passes spinners instantly
            if (ch.type == CharacterType::Friday) {
                if (isSpinner(tile.type)) {
                    // Friday passes spinners in 1 turn — just land, no spinner progress
                    events.push_back({EventType::SpinnerAdvanced, move.to, {}, {}, ch.owner, {}, spinnerSteps(tile.type)});
                }
                // Friday ignores traps — do nothing for Trap tiles
                // Friday meets Rum tile → disappears
                if (tile.type == TileType::Rum && tile.rumBottles > 0) {
                    ch.alive = false; ch.pos = {-1, -1};
                    tile.rumBottles = 0;
                    events.push_back({EventType::CharacterDied, move.to, {}, {}, ch.owner, {}, ci});
                    state.advanceTurn(); break;
                }
            }

            // Dangerous tiles
            if (tile.type == TileType::Cannibal && ch.type != CharacterType::Friday) {
                ch.alive = false; ch.pos = {-1, -1};
                events.push_back({EventType::CharacterDied, move.to, {}, {}, ch.owner, {}, ci});
                state.advanceTurn(); break;
            }
            if (tile.type == TileType::Trap && ch.type != CharacterType::Friday) {
                // Non-Friday characters get trapped (simplified)
                // For now just stay there — they need rescue like pirates
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

    case MoveType::UseRum: {
        int ti = static_cast<int>(move.pirateId.team);
        if (state.rumOwned[ti] <= 0) { state.advanceTurn(); break; }

        // Case 1: Free trapped/caved pirate
        if (move.characterIndex < 0) {
            auto& p = state.pirateRef(move.pirateId);
            if (p.state == PirateState::InTrap) {
                p.state = PirateState::OnBoard;
                state.rumOwned[ti]--;
                events.push_back({EventType::RumUsed, p.pos, {}, p.id, p.id.team});
                events.push_back({EventType::PirateFreed, p.pos, {}, p.id});
            } else if (p.state == PirateState::InCave) {
                // Free from cave — pirate appears at the cave tile entrance
                p.state = PirateState::OnBoard;
                state.rumOwned[ti]--;
                events.push_back({EventType::RumUsed, p.pos, {}, p.id, p.id.team});
                events.push_back({EventType::CaveExited, {-1,-1}, p.pos, p.id});
            }
        }
        // Case 2: Use rum on a character (Friday or Missionary)
        else {
            int ci = move.characterIndex;
            if (ci >= 0 && ci < MAX_CHARACTERS) {
                auto& ch = state.characters[ci];
                state.rumOwned[ti]--;

                if (ch.type == CharacterType::Friday) {
                    // Friday drinks rum → disappears from game
                    ch.alive = false;
                    ch.pos = {-1, -1};
                    events.push_back({EventType::RumUsed, move.to, {}, move.pirateId, move.pirateId.team});
                    events.push_back({EventType::CharacterDied, move.to, {}, {}, ch.owner, {}, ci});
                }
                else if (ch.type == CharacterType::Missionary) {
                    // Missionary drinks rum → becomes regular pirate of his team
                    // Find a dead pirate slot to revive as the missionary
                    Team mTeam = ch.owner;
                    int mt = static_cast<int>(mTeam);
                    bool converted = false;
                    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                        if (state.pirates[mt][i].state == PirateState::Dead) {
                            state.pirates[mt][i].state = PirateState::OnBoard;
                            state.pirates[mt][i].pos = ch.pos;
                            state.pirates[mt][i].carryingCoin = false;
                            state.pirates[mt][i].carryingGalleon = false;
                            state.pirates[mt][i].spinnerProgress = 0;
                            state.pirates[mt][i].drunkTurnsLeft = 0;
                            converted = true;
                            events.push_back({EventType::PirateResurrected, ch.pos, {},
                                state.pirates[mt][i].id, mTeam});
                            break;
                        }
                    }
                    ch.alive = false;
                    ch.pos = {-1, -1};
                    events.push_back({EventType::RumUsed, move.to, {}, move.pirateId, move.pirateId.team});
                    events.push_back({EventType::CharacterDied, move.to, {}, {}, ch.owner, {}, ci});
                }
            }
        }
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
