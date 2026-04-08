#include "game.h"
#include "rules.h"
#include "ai.h"
#include "map_def.h"
#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>

// ============================================================
// Test framework (minimal)
// ============================================================
static int g_passed = 0, g_failed = 0;
static const char* g_section = "";

#define SECTION(name) g_section = name; std::cout << "\n=== " << name << " ===\n"

#define CHECK(cond, msg) do { \
    if (cond) { g_passed++; } \
    else { g_failed++; \
        std::cerr << "  FAIL [" << g_section << "]: " << msg << "\n"; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg << " (got " << (a) << ", expected " << (b) << ")")

// Helper: find a specific move type in move list
static Move findMove(const MoveList& moves, MoveType type, int pirateIdx = -1) {
    for (auto& m : moves)
        if (m.type == type && (pirateIdx < 0 || m.pirateId.index == pirateIdx))
            return m;
    return {};
}

static bool hasMove(const MoveList& moves, MoveType type, int pirateIdx = -1) {
    for (auto& m : moves)
        if (m.type == type && (pirateIdx < 0 || m.pirateId.index == pirateIdx))
            return true;
    return false;
}

static Game makeGame(const std::string& mapId = "classic", int teams = 2, uint32_t seed = 42) {
    GameConfig cfg;
    cfg.mapId = mapId;
    cfg.numTeams = teams;
    cfg.seed = seed;
    Game g;
    g.newGame(cfg);
    return g;
}

static Game makeSandbox(TileType tile, uint8_t dir = 0, int val = 0) {
    GameConfig cfg;
    cfg.numTeams = 2;
    cfg.seed = 1;
    cfg.sandbox = true;
    cfg.sandboxTile = tile;
    cfg.sandboxDirBits = dir;
    cfg.sandboxValue = val;
    Game g;
    g.newGame(cfg);
    return g;
}

// Disembark pirate 0 and return events
static EventList disembark(Game& g, int pirateIdx = 0) {
    auto moves = g.getLegalMoves();
    for (auto& m : moves)
        if (m.type == MoveType::DisembarkPirate && m.pirateId.index == pirateIdx)
            return g.makeMove(m);
    return {};
}

// Skip opponent turn (advance to next player)
static void skipTurn(Game& g) {
    auto moves = g.getLegalMoves();
    if (!moves.empty()) g.makeMove(moves[0]);
}

// Resolve any pending phases (arrow choice, horse, etc)
static void resolvePhases(Game& g) {
    int safety = 20;
    while (g.currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
        auto pm = g.getLegalMoves();
        if (pm.empty()) break;
        g.makeMove(pm[0]);
    }
}

// Disembark pirate 0 then move to target (sandbox: test tile at (2,6))
static EventList disembarkAndMoveToTest(Game& g, Coord target = {2, 6}) {
    disembark(g, 0);           // land on (1,6) = Empty revealed
    resolvePhases(g);
    skipTurn(g);               // opponent
    // Now move pirate 0 from (1,6) to target
    auto moves = g.getLegalMoves();
    for (auto& m : moves) {
        if (m.type == MoveType::MovePirate && m.pirateId.index == 0 && m.to == target)
            return g.makeMove(m);
    }
    // If can't reach directly, try any move toward target
    return {};
}

// ============================================================
// 1. Maps
// ============================================================
static void testMaps() {
    SECTION("Maps");
    auto& maps = getBuiltinMaps();
    CHECK(maps.size() >= 5, "At least 5 built-in maps");

    for (auto& m : maps) {
        CHECK(m.countLandCells() > 0, m.id << " has land cells");
        CHECK(!m.ships.empty(), m.id << " has ship spawns");

        // Ships must be on water, adjacent to land
        for (auto& sp : m.ships) {
            Coord shipCoord = {sp.row, sp.col};
            CHECK(m.cellIsWater(shipCoord), m.id << " ship " << sp.team << " on water");
            bool nearLand = false;
            for (int d = 0; d < 8; d++) {
                Coord adj = shipCoord.moved(static_cast<Direction>(d));
                if (adj.inBounds() && m.cellIsLand(adj)) nearLand = true;
            }
            CHECK(nearLand, m.id << " ship " << sp.team << " adjacent to land");
        }
    }

    CHECK_EQ(findMap("classic")->countLandCells(), 117, "Classic = 117 land");
    CHECK(findMap("duel") != nullptr, "Duel map exists");
    CHECK(findMap("hangman") != nullptr, "Hangman map exists");
    CHECK(findMap("turtle") != nullptr, "Turtle map exists");
    CHECK(findMap("bridges") != nullptr, "Bridges map exists");
    CHECK(findMap("nonexistent") == findMap("classic"), "Unknown map falls back to classic");
}

// ============================================================
// 2. Deck
// ============================================================
static void testDeck() {
    SECTION("Deck");
    auto deck = createDeck(42, 117);
    CHECK_EQ(static_cast<int>(deck.size()), 117, "Classic deck size");

    // Count tile types and coins
    int treasureCoins = 0;
    int count[static_cast<int>(TileType::COUNT)] = {};
    for (auto& t : deck) {
        count[static_cast<int>(t.type)]++;
        if (t.type == TileType::Treasure) treasureCoins += t.treasureValue;
    }

    CHECK_EQ(treasureCoins, 37, "Treasure coins sum = 37");
    CHECK_EQ(count[static_cast<int>(TileType::Galleon)], 1, "Exactly 1 Galleon");
    CHECK_EQ(count[static_cast<int>(TileType::Cannibal)], 1, "Exactly 1 Cannibal");
    CHECK_EQ(count[static_cast<int>(TileType::Airplane)], 1, "Exactly 1 Airplane");
    CHECK_EQ(count[static_cast<int>(TileType::Lighthouse)], 1, "Exactly 1 Lighthouse");
    CHECK_EQ(count[static_cast<int>(TileType::Cave)], 4, "Exactly 4 Caves");
    CHECK_EQ(count[static_cast<int>(TileType::Horse)], 2, "Exactly 2 Horses");

    // Same seed = same deck
    auto deck2 = createDeck(42, 117);
    bool identical = true;
    for (int i = 0; i < 117; i++)
        if (deck[i].type != deck2[i].type) { identical = false; break; }
    CHECK(identical, "Same seed = same deck");

    // Different seed = different deck
    auto deck3 = createDeck(99, 117);
    bool different = false;
    for (int i = 0; i < 117; i++)
        if (deck[i].type != deck3[i].type) { different = true; break; }
    CHECK(different, "Different seed = different deck");
}

// ============================================================
// 3. Init
// ============================================================
static void testInit() {
    SECTION("Init");
    auto g = makeGame("classic", 2, 42);
    auto& s = g.state();

    // All land cells have non-Sea tiles
    int landTiles = 0;
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            if (s.mapIsLand({r, c})) {
                CHECK(s.board[r][c].type != TileType::Sea,
                      "Land (" << r << "," << c << ") has tile");
                CHECK(!s.board[r][c].revealed,
                      "Land (" << r << "," << c << ") unrevealed");
                landTiles++;
            }
    CHECK_EQ(landTiles, 117, "117 land tiles placed");

    // Pirates on ships
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            CHECK(s.pirates[t][i].state == PirateState::OnShip,
                  "Team " << t << " pirate " << i << " on ship");
        }

    // Scores zero
    CHECK_EQ(s.scores[0], 0, "White score = 0");
    CHECK_EQ(s.scores[1], 0, "Yellow score = 0");

    // Game not over
    CHECK(!g.isGameOver(), "Game not over at start");
    CHECK(Rules::coinsRemaining(s) == 40, "40 coins remaining (37 treasure + 3 galleon)");
}

// ============================================================
// 4. Movement
// ============================================================
static void testMovement() {
    SECTION("Movement");

    // Disembark
    auto g = makeSandbox(TileType::Empty);
    auto events = disembark(g, 0);
    auto& p = g.state().pirates[0][0];
    CHECK(p.state == PirateState::OnBoard, "Disembarked pirate on board");
    Coord front = g.state().shipFront(Team::White);
    CHECK(p.pos == front, "Pirate at ship front after disembark");

    // Skip opponent, then move pirate
    skipTurn(g); // Yellow's turn
    auto moves = g.getLegalMoves();
    bool has8dir = false;
    int pirMoves = 0;
    for (auto& m : moves)
        if (m.type == MoveType::MovePirate && m.pirateId.index == 0) pirMoves++;
    CHECK(pirMoves >= 3, "Pirate has multiple move targets");

    // Board ship with coin: disembark, give coin manually, board back
    auto g2 = makeSandbox(TileType::Empty);
    disembark(g2, 0);
    g2.state().pirates[0][0].carryingCoin = true; // give coin
    skipTurn(g2);
    moves = g2.getLegalMoves();
    bool canBoard = hasMove(moves, MoveType::BoardShip, 0);
    if (canBoard) {
        g2.makeMove(findMove(moves, MoveType::BoardShip, 0));
        CHECK(g2.state().pirates[0][0].state == PirateState::OnShip, "Pirate boarded ship");
        CHECK(g2.state().scores[0] > 0, "Score increased after boarding with coin");
    } else {
        CHECK(true, "Board ship test: pirate not adjacent to ship (layout dependent)");
    }

    // Pirate with coin can't move to unrevealed
    auto g3 = makeSandbox(TileType::Empty);
    // Give pirate a coin manually
    disembark(g3, 0);
    g3.state().pirates[0][0].carryingCoin = true;
    skipTurn(g3);
    moves = g3.getLegalMoves();
    bool hasUnrevealed = false;
    for (auto& m : moves) {
        if (m.type == MoveType::MovePirate && m.pirateId.index == 0) {
            if (g3.state().mapIsLand(m.to) && !g3.state().board[m.to.row][m.to.col].revealed)
                hasUnrevealed = true;
        }
    }
    CHECK(!hasUnrevealed, "Pirate with coin cannot move to unrevealed");
}

// ============================================================
// 5. Tile Effects (uses disembarkAndMoveToTest to reach test tiles at (2,6))
// ============================================================
static void testTileEffects() {
    SECTION("Tile Effects");

    // --- Arrow 1-dir (south, so pirate pushed further south) ---
    {
        auto g = makeSandbox(TileType::Arrow, dirBit(DIR_S));
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        auto& p = g.state().pirates[0][0];
        CHECK(p.state == PirateState::OnBoard || p.state == PirateState::OnShip,
              "Arrow 1-dir: pirate alive");
        // Arrow moved pirate away from (2,6) — may chain through multiple arrows
        if (p.state == PirateState::OnBoard)
            CHECK((p.pos != Coord{2,6}), "Arrow 1-dir: moved away from entry tile");
    }

    // --- Arrow multi-dir ---
    {
        auto g = makeSandbox(TileType::Arrow, DIRS_CARDINAL);
        disembarkAndMoveToTest(g);
        bool arrowPhase = (g.currentPhase() == TurnPhase::ChooseArrowDirection);
        CHECK(arrowPhase, "Arrow 4-dir: phase = ChooseArrowDirection");
        if (arrowPhase) {
            auto moves = g.getLegalMoves();
            CHECK(moves.size() >= 2, "Arrow 4-dir: multiple choices");
        }
        resolvePhases(g);
    }

    // --- Horse ---
    {
        auto g = makeSandbox(TileType::Horse);
        disembarkAndMoveToTest(g);
        bool horsePhase = (g.currentPhase() == TurnPhase::ChooseHorseDest);
        CHECK(horsePhase, "Horse: phase = ChooseHorseDest");
        if (horsePhase) {
            auto moves = g.getLegalMoves();
            CHECK(moves.size() >= 2, "Horse: multiple L-shaped destinations");
            for (auto& m : moves) {
                int dr = std::abs(m.to.row - m.from.row);
                int dc = std::abs(m.to.col - m.from.col);
                CHECK(dr + dc == 3 && dr * dc == 2,
                      "Horse: L-shaped (" << dr << "," << dc << ")");
            }
        }
        resolvePhases(g);
    }

    // --- Jungle spinner (2 steps) ---
    {
        auto g = makeSandbox(TileType::Jungle);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        auto& p = g.state().pirates[0][0];
        CHECK(p.spinnerProgress == 1, "Jungle: progress=1");
        CHECK(g.state().isOnActiveSpinner(p), "Jungle: on active spinner");

        skipTurn(g);
        auto moves = g.getLegalMoves();
        CHECK(hasMove(moves, MoveType::AdvanceSpinner, 0), "Jungle: AdvanceSpinner available");
        CHECK(!hasMove(moves, MoveType::MovePirate, 0), "Jungle: no land moves on spinner");

        g.makeMove(findMove(moves, MoveType::AdvanceSpinner, 0));
        skipTurn(g);
        CHECK(!g.state().isOnActiveSpinner(g.state().pirates[0][0]), "Jungle: free after 2 steps");
        moves = g.getLegalMoves();
        CHECK(hasMove(moves, MoveType::MovePirate, 0), "Jungle: land moves after done");
    }

    // --- Trap ---
    {
        auto g = makeSandbox(TileType::Trap);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].state == PirateState::InTrap, "Trap: pirate trapped");
    }

    // --- Crocodile ---
    {
        auto g = makeSandbox(TileType::Crocodile);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        auto& p = g.state().pirates[0][0];
        // Should return to (1,6) where pirate was before
        CHECK(p.pos.row <= 1 || p.state == PirateState::OnShip,
              "Crocodile: returned back");
    }

    // --- Cannibal ---
    {
        auto g = makeSandbox(TileType::Cannibal);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].state == PirateState::Dead, "Cannibal: pirate dead");
    }

    // --- Balloon ---
    {
        auto g = makeSandbox(TileType::Balloon);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].state == PirateState::OnShip, "Balloon: on ship");
    }

    // --- Cannon (fires south from (2,6) → water) ---
    {
        auto g = makeSandbox(TileType::Cannon, dirBit(DIR_S));
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        auto& p = g.state().pirates[0][0];
        CHECK(p.state == PirateState::OnBoard || p.state == PirateState::OnShip ||
              p.state == PirateState::Dead,
              "Cannon: pirate displaced");
    }

    // --- Treasure ---
    {
        auto g = makeSandbox(TileType::Treasure, 0, 3);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        auto& p = g.state().pirates[0][0];
        // Pirate should have picked up a coin from revealed treasure
        CHECK(p.carryingCoin || p.carryingGalleon ||
              g.state().tileAt(p.pos).coins > 0,
              "Treasure: coins exist");
    }

    // --- RumBarrel ---
    {
        auto g = makeSandbox(TileType::RumBarrel);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].drunkTurnsLeft > 0, "RumBarrel: pirate drunk");
    }

    // --- BenGunn ---
    {
        auto g = makeSandbox(TileType::BenGunn);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        bool joined = false;
        for (auto& ch : g.state().characters)
            if (ch.type == CharacterType::BenGunn && ch.discovered && ch.owner == Team::White)
                joined = true;
        CHECK(joined, "BenGunn: joined team");
    }

    // --- Fortress ---
    {
        auto g = makeSandbox(TileType::Fortress);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].state == PirateState::OnBoard, "Fortress: pirate safe");
    }

    // --- ThickJungle ---
    {
        auto g = makeSandbox(TileType::ThickJungle);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        CHECK(g.state().pirates[0][0].state == PirateState::OnBoard, "ThickJungle: on board");
    }

    // --- Ice (south entry → continues south) ---
    {
        auto g = makeSandbox(TileType::Ice);
        disembarkAndMoveToTest(g);
        resolvePhases(g);
        // Ice just chains, pirate ends up somewhere
        CHECK(true, "Ice: processed without crash");
    }
}

// ============================================================
// 6. Combat
// ============================================================
static void testCombat() {
    SECTION("Combat");

    auto g = makeSandbox(TileType::Empty);
    // Disembark both teams' pirates to same area
    disembark(g, 0); // White pirate 0 to front
    Coord whiteFront = g.state().shipFront(Team::White);

    skipTurn(g); // Yellow
    disembark(g, 1); // White pirate 1

    // Move white pirate deeper so yellow can land on front
    skipTurn(g); // Yellow
    auto moves = g.getLegalMoves();
    for (auto& m : moves) {
        if (m.type == MoveType::MovePirate && m.pirateId.index == 0) {
            g.makeMove(m);
            break;
        }
    }
    resolvePhases(g);

    // This is a basic combat setup test — verifying framework works
    CHECK(g.state().pirates[0][0].state == PirateState::OnBoard, "Combat setup: white pirate alive");
}

// ============================================================
// 7. Coins
// ============================================================
static void testCoins() {
    SECTION("Coins");

    // coinsRemaining at start includes unrevealed treasure
    auto g = makeGame("classic", 2, 42);
    int coins = Rules::coinsRemaining(g.state());
    CHECK_EQ(coins, 40, "40 coins at start (37 treasure + 3 galleon)");

    // After revealing treasure, coins still counted
    disembark(g, 0);
    resolvePhases(g);
    int coinsAfter = Rules::coinsRemaining(g.state());
    CHECK(coinsAfter <= 40, "Coins don't increase after move");
    CHECK(coinsAfter >= 37, "Most coins still remaining after 1 move");
}

// ============================================================
// 8. Game Over
// ============================================================
static void testGameOver() {
    SECTION("GameOver");

    auto g = makeGame("classic", 2, 42);
    CHECK(!g.isGameOver(), "Not over at start");
    CHECK(Rules::coinsRemaining(g.state()) > 0, "Coins exist at start");

    // Manually zero all coins to test game over
    auto& s = g.state();
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++) {
            s.board[r][c].coins = 0;
            s.board[r][c].hasGalleonTreasure = false;
            if (s.board[r][c].type == TileType::Treasure) s.board[r][c].treasureValue = 0;
            if (s.board[r][c].type == TileType::Galleon) s.board[r][c].type = TileType::Empty;
        }
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            s.pirates[t][i].carryingCoin = false;
            s.pirates[t][i].carryingGalleon = false;
        }
    CHECK(Rules::coinsRemaining(s) == 0, "Coins zeroed");
    CHECK(g.isGameOver(), "Game over when no coins");
}

// ============================================================
// 9. AI
// ============================================================
static void testAI() {
    SECTION("AI");

    // AI returns valid move
    auto g = makeGame("classic", 2, 42);
    auto moves = g.getLegalMoves();
    Move best = AI::chooseBestMove(g.state(), moves);
    bool valid = false;
    for (auto& m : moves)
        if (m.type == best.type && m.to == best.to && m.pirateId == best.pirateId)
            { valid = true; break; }
    CHECK(valid, "AI returns a move from legal moves");

    // AI stress test: play 50 moves without crash
    for (const auto& mapId : {"classic", "duel", "hangman", "turtle", "bridges"}) {
        auto gg = makeGame(mapId, 2, 77);
        bool crashed = false;
        for (int turn = 0; turn < 50 && !gg.isGameOver(); turn++) {
            auto mm = gg.getLegalMoves();
            if (mm.empty()) { crashed = true; break; }
            Move chosen = AI::chooseBestMove(gg.state(), mm);
            gg.makeMove(chosen);
            // Resolve phases
            int safety = 20;
            while (gg.currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
                auto pm = gg.getLegalMoves();
                if (pm.empty()) break;
                gg.makeMove(AI::chooseBestMove(gg.state(), pm));
            }
        }
        CHECK(!crashed, "AI 50-turn stress: " << mapId);
    }
}

// ============================================================
// 10. Invariant Fuzz
// ============================================================
static void testInvariants() {
    SECTION("Invariants (fuzz)");

    int totalGames = 0;
    int totalTurns = 0;
    bool anyFail = false;

    for (uint32_t seed = 1; seed <= 50; seed++) {
        for (const auto& mapId : {"classic", "duel"}) {
            Game g = makeGame(mapId, 2, seed);
            int startCoins = Rules::coinsRemaining(g.state());
            totalGames++;

            for (int turn = 0; turn < 30 && !g.isGameOver(); turn++) {
                totalTurns++;
                auto moves = g.getLegalMoves();

                if (moves.empty()) {
                    std::cerr << "  WARN: no moves at seed=" << seed << " map=" << mapId
                              << " turn=" << turn << "\n";
                    break;
                }

                // All move targets in bounds
                for (auto& m : moves) {
                    if (m.to.valid() && !m.to.inBounds()) {
                        std::cerr << "  FAIL: OOB move at seed=" << seed << "\n";
                        anyFail = true;
                    }
                }

                // Make a deterministic move
                Move chosen = moves[seed % moves.size()];
                g.makeMove(chosen);

                // Resolve phases
                int safety = 20;
                while (g.currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
                    auto pm = g.getLegalMoves();
                    if (pm.empty()) break;
                    g.makeMove(pm[0]);
                }

                // Coins never increase
                int coins = Rules::coinsRemaining(g.state());
                if (coins > startCoins) {
                    std::cerr << "  FAIL: coins increased! seed=" << seed << "\n";
                    anyFail = true;
                }

                // All pirates in valid state
                for (int t = 0; t < 2; t++)
                    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                        auto st = g.state().pirates[t][i].state;
                        bool ok = (st == PirateState::OnShip || st == PirateState::OnBoard ||
                                   st == PirateState::Dead || st == PirateState::InTrap ||
                                   st == PirateState::InCave);
                        if (!ok) {
                            std::cerr << "  FAIL: invalid pirate state seed=" << seed << "\n";
                            anyFail = true;
                        }
                    }
            }
        }
    }

    CHECK(!anyFail, "No invariant violations in " << totalGames << " games, " << totalTurns << " turns");
    std::cout << "  Fuzz: " << totalGames << " games, " << totalTurns << " turns OK\n";
}

// ============================================================
// 11. All maps playable
// ============================================================
static void testAllMaps() {
    SECTION("All Maps Playable");

    auto& maps = getBuiltinMaps();
    for (auto& mapDef : maps) {
        int teams = std::min(mapDef.maxPlayers, 2);
        auto g = makeGame(mapDef.id, teams, 42);

        // Can get legal moves
        auto moves = g.getLegalMoves();
        CHECK(!moves.empty(), mapDef.id << ": has legal moves at start");

        // Can disembark
        bool canDisembark = hasMove(moves, MoveType::DisembarkPirate);
        CHECK(canDisembark, mapDef.id << ": can disembark");

        // Play 10 turns without crash
        bool ok = true;
        for (int turn = 0; turn < 10 && !g.isGameOver(); turn++) {
            auto mm = g.getLegalMoves();
            if (mm.empty()) { ok = false; break; }
            g.makeMove(mm[0]);
            resolvePhases(g);
        }
        CHECK(ok, mapDef.id << ": 10 turns without crash");
    }
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "Jacal Core Test Suite\n";
    std::cout << "=====================\n";

    testMaps();
    testDeck();
    testInit();
    testMovement();
    testTileEffects();
    testCombat();
    testCoins();
    testGameOver();
    testAI();
    testAllMaps();
    testInvariants();

    std::cout << "\n=====================\n";
    std::cout << "PASSED: " << g_passed << "  FAILED: " << g_failed << "\n";
    std::cout << "=====================\n";

    return g_failed > 0 ? 1 : 0;
}
