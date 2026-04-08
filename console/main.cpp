#include "game.h"
#include "rules.h"
#include "ai.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <array>

#ifdef _WIN32
#include <windows.h>
#endif

// ANSI color codes
#define RST  "\033[0m"
#define BOLD "\033[1m"
#define DIM  "\033[2m"
#define FG_RED     "\033[31m"
#define FG_GREEN   "\033[32m"
#define FG_YELLOW  "\033[33m"
#define FG_CYAN    "\033[36m"
#define FG_WHITE   "\033[37m"
#define FG_GRAY    "\033[90m"
#define FG_BRIGHT_RED    "\033[91m"
#define FG_BRIGHT_GREEN  "\033[92m"
#define FG_BRIGHT_YELLOW "\033[93m"
#define BG_BLUE    "\033[44m"
#define BG_GREEN   "\033[42m"
#define BG_DGREEN  "\033[48;5;22m"
#define BG_YELLOW  "\033[43m"
#define BG_DGRAY   "\033[100m"
#define BG_BROWN   "\033[48;5;94m"
#define BG_CYAN    "\033[46m"
#define BG_RED     "\033[41m"
#define BG_DARK    "\033[40m"

static void enableAnsiColors() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

static const char* teamAnsi(Team t) {
    switch (t) {
        case Team::White:  return FG_WHITE BOLD;
        case Team::Yellow: return FG_BRIGHT_YELLOW BOLD;
        case Team::Black:  return FG_GRAY BOLD;
        case Team::Red:    return FG_BRIGHT_RED BOLD;
        default: return RST;
    }
}

static char teamLetter(Team t) {
    switch (t) { case Team::White: return 'W'; case Team::Yellow: return 'Y';
        case Team::Black: return 'B'; case Team::Red: return 'R'; default: return '?'; }
}

static const char* tileBg(TileType type, bool revealed) {
    if (!revealed) return BG_DGREEN;
    switch (type) {
        case TileType::Empty: case TileType::Grass: return BG_GREEN;
        case TileType::Arrow: case TileType::Horse: return BG_YELLOW;
        case TileType::Jungle: case TileType::ThickJungle: case TileType::Crocodile: return BG_DGREEN;
        case TileType::Desert: case TileType::Treasure: case TileType::Galleon:
        case TileType::Rum: case TileType::RumBarrel: return BG_BROWN;
        case TileType::Mountain: case TileType::Fortress: case TileType::ResurrectFort:
        case TileType::Cannon: return BG_DGRAY;
        case TileType::Ice: case TileType::Balloon: return BG_CYAN;
        case TileType::Trap: case TileType::Cannibal: return BG_RED;
        case TileType::Cave: return BG_DARK;
        default: return BG_GREEN;
    }
}

static void printBoard(const Game& game) {
    const auto& s = game.state();
    std::cout << "\n     ";
    for (int c = 0; c < BOARD_SIZE; c++) std::cout << std::setw(4) << c << " ";
    std::cout << "\n";

    for (int r = 0; r < BOARD_SIZE; r++) {
        std::cout << "  " << std::setw(2) << r << " ";
        for (int c = 0; c < BOARD_SIZE; c++) {
            Coord coord = {r, c};
            if (isCorner(coord)) { std::cout << BG_DARK "     " RST; continue; }

            if (isWater(coord)) {
                int si = s.shipIndexAt(coord);
                if (si >= 0) {
                    Team st = static_cast<Team>(si);
                    int n = 0;
                    for (int i = 0; i < PIRATES_PER_TEAM; i++)
                        if (s.pirates[si][i].state == PirateState::OnShip) n++;
                    std::cout << BG_BLUE << teamAnsi(st) << " S" << teamLetter(st) << n << " " RST;
                } else {
                    auto pids = s.piratesAt(coord);
                    if (!pids.empty()) {
                        std::cout << BG_BLUE << teamAnsi(pids[0].team) << " "
                                  << teamLetter(pids[0].team) << "~  " RST;
                    } else {
                        std::cout << BG_BLUE FG_CYAN "  ~  " RST;
                    }
                }
                continue;
            }

            // Land tile
            const auto& tile = s.board[r][c];
            std::cout << tileBg(tile.type, tile.revealed);

            if (!tile.revealed) { std::cout << FG_GREEN " ??? " RST; continue; }

            char cell[6] = "     ";
            if (tile.type == TileType::Arrow) {
                uint8_t d = tile.directionBits;
                if (d & dirBit(DIR_N))  cell[2] = '^';
                else if (d & dirBit(DIR_S))  cell[2] = 'v';
                else if (d & dirBit(DIR_E))  cell[2] = '>';
                else if (d & dirBit(DIR_W))  cell[2] = '<';
                else if (d & dirBit(DIR_NE)) cell[2] = '/';
                else if (d & dirBit(DIR_SW)) cell[2] = '/';
                else if (d & dirBit(DIR_NW)) cell[2] = '\\';
                else if (d & dirBit(DIR_SE)) cell[2] = '\\';
                int n = popcount8(d);
                if (n > 1) { cell[0] = '0'+n; cell[1] = 'd'; }
            } else if (tile.type == TileType::Treasure) {
                cell[0] = '$'; cell[1] = '0' + tile.coins;
            } else {
                const char* sym = tileSymbol(tile.type);
                for (int i = 0; sym[i] && i < 3; i++) cell[i] = sym[i];
            }

            // Pirates overlay
            auto pids = s.piratesAt(coord);
            if (!pids.empty()) {
                int pos = (cell[3] == ' ') ? 3 : 4;
                for (auto& pid : pids) { if (pos < 5) cell[pos++] = teamLetter(pid.team); }
            }

            std::cout << FG_WHITE << std::string(cell, 5) << RST;
        }
        std::cout << "  " << r << "\n";
    }
    std::cout << "     ";
    for (int c = 0; c < BOARD_SIZE; c++) std::cout << std::setw(4) << c << " ";
    std::cout << "\n";
}

static void printScores(const Game& game) {
    const auto& s = game.state();
    std::cout << "\n" BOLD "=== " << teamAnsi(s.currentTeam()) << teamName(s.currentTeam())
              << RST BOLD " === Ход " << game.turnNumber()
              << " === Монет на острове: " << Rules::coinsRemaining(s) << " ===" RST "\n";
    std::cout << "Счёт: ";
    for (int t = 0; t < s.config.numTeams; t++) {
        std::cout << teamAnsi(static_cast<Team>(t)) << teamName(static_cast<Team>(t))
                  << RST ":" << s.scores[t] << "  ";
    }
    std::cout << "\nПираты: ";
    int ti = static_cast<int>(s.currentTeam());
    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = s.pirates[ti][i];
        std::cout << "#" << i << ":";
        switch (p.state) {
            case PirateState::OnShip: std::cout << "корабль"; break;
            case PirateState::OnBoard:
                std::cout << "(" << p.pos.row << "," << p.pos.col << ")";
                if (p.carryingCoin) std::cout << "$";
                if (p.carryingGalleon) std::cout << "$$$";
                if (p.drunkTurnsLeft > 0) std::cout << FG_YELLOW "(пьян)" RST;
                if (p.spinnerProgress > 0) std::cout << "[" << p.spinnerProgress << "]";
                break;
            case PirateState::Dead: std::cout << FG_RED "мёртв" RST; break;
            case PirateState::InTrap: std::cout << FG_YELLOW "ловушка" RST; break;
            case PirateState::InCave: std::cout << FG_GRAY "пещера" RST; break;
        }
        std::cout << "  ";
    }
    std::cout << "\n";
}

static void printEvents(const EventList& events) {
    for (auto& ev : events) {
        switch (ev.type) {
            case EventType::TileRevealed:
                std::cout << FG_CYAN "  >> Открыта: " << tileTypeName(ev.tileType) << RST "\n"; break;
            case EventType::PirateAttacked:
                std::cout << FG_RED "  >> Атака! Враг на корабль" RST "\n"; break;
            case EventType::PirateDied:
                std::cout << FG_BRIGHT_RED BOLD "  >> ПИРАТ ПОГИБ!" RST "\n"; break;
            case EventType::CoinLoaded:
                std::cout << FG_BRIGHT_YELLOW "  >> Монета на корабль! (+" << (ev.value > 0 ? ev.value : 1) << ")" RST "\n"; break;
            case EventType::CoinPickedUp:
                std::cout << FG_YELLOW "  >> Монета подобрана" RST "\n"; break;
            case EventType::CoinLost:
                std::cout << FG_RED "  >> Монета утонула!" RST "\n"; break;
            case EventType::PirateTrapped:
                std::cout << FG_YELLOW "  >> Ловушка!" RST "\n"; break;
            case EventType::PirateFreed:
                std::cout << FG_GREEN "  >> Освобождён!" RST "\n"; break;
            case EventType::BalloonLiftoff:
                std::cout << FG_CYAN "  >> Шар! На корабль!" RST "\n"; break;
            case EventType::CannonFired:
                std::cout << FG_RED "  >> ПУШКА!" RST "\n"; break;
            case EventType::CharacterJoined:
                std::cout << FG_GREEN BOLD "  >> Персонаж присоединился!" RST "\n"; break;
            case EventType::SpinnerAdvanced:
                std::cout << FG_GRAY "  >> Вертушка: шаг " << ev.value << RST "\n"; break;
            case EventType::PirateDrunk:
                std::cout << FG_YELLOW "  >> Ром! Пропуск хода" RST "\n"; break;
            case EventType::PirateResurrected:
                std::cout << FG_GREEN "  >> Пират воскрешён!" RST "\n"; break;
            default: break;
        }
    }
}

static int readInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; exit(0); }
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        if (line == "q" || line == "Q") exit(0);
        try {
            int v = std::stoi(line);
            if (v >= minVal && v <= maxVal) return v;
        } catch (...) {}
        std::cout << "  Число " << minVal << "-" << maxVal << " (или q)\n";
    }
}

static std::string moveDescription(const Move& m) {
    std::string s;
    switch (m.type) {
    case MoveType::MoveShip:
        s = "Корабль -> (" + std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::DisembarkPirate:
        s = "Высадить #" + std::to_string(m.pirateId.index) + " -> (" +
            std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::MovePirate:
        s = "Пират #" + std::to_string(m.pirateId.index) + " (" +
            std::to_string(m.from.row) + "," + std::to_string(m.from.col) + ") -> (" +
            std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::BoardShip:
        s = "Пират #" + std::to_string(m.pirateId.index) + " -> на корабль"; break;
    case MoveType::AdvanceSpinner:
        s = "Пират #" + std::to_string(m.pirateId.index) + " -> вертушка"; break;
    case MoveType::ResurrectPirate:
        s = "Воскресить #" + std::to_string(m.pirateId.index); break;
    case MoveType::ChooseDirection:
        s = "Направление -> (" + std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::ChooseHorseDest:
        s = "Конь -> (" + std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::ChooseCaveExit:
        s = "Пещера -> (" + std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")"; break;
    case MoveType::SkipDrunk:
        s = "Пропуск (ром)"; break;
    case MoveType::PickupCoin:
        s = "Подобрать монету"; break;
    case MoveType::UseRum:
        if (m.characterIndex >= 0)
            s = "Ром -> персонаж (" + std::to_string(m.to.row) + "," + std::to_string(m.to.col) + ")";
        else
            s = "Ром -> освободить #" + std::to_string(m.pirateId.index);
        break;
    default:
        s = "Ход #" + std::to_string(static_cast<int>(m.type)); break;
    }
    return s;
}

int main() {
    enableAnsiColors();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    std::cout << BOLD FG_BRIGHT_YELLOW R"(
       ___   ______   ______   ______   __
      / / | / ____/  / ____/  / __  /  / /
     / /| |/ /      / /___   / /_/ /  / /
    / / | / /      / ____/  / __  /  / /
   / /  |/ /___   / /___   / / / /  / /___
  /_/   |\_____/  \_____/  /_/ /_/  /_____/
)" RST "\n";
    std::cout << BOLD "     Jacal — Treasure Island\n" RST;
    std::cout << "     ======================\n\n";

    std::cout << "Режим:\n";
    std::cout << "  1. Один игрок vs ИИ бот\n";
    std::cout << "  2. Два игрока (hot-seat, команды)\n";
    std::cout << "  3. Три игрока (hot-seat)\n";
    std::cout << "  4. Четыре игрока (hot-seat)\n";
    int mode = readInt("\nВыбор [1-4]: ", 1, 4);

    GameConfig cfg;
    std::array<bool, MAX_TEAMS> isAI = {false, false, false, false};

    switch (mode) {
    case 1:
        cfg.numTeams = 4; cfg.teamMode = true;
        // Human = White+Black (teams 0,2), AI = Yellow+Red (teams 1,3)
        isAI = {false, true, false, true};
        std::cout << FG_GREEN "\nВы играете за Белых + Чёрных. ИИ за Жёлтых + Красных.\n" RST;
        break;
    case 2:
        cfg.numTeams = 4; cfg.teamMode = true;
        std::cout << FG_GREEN "\nКоманды: Белые+Чёрные vs Жёлтые+Красные.\n" RST;
        break;
    case 3:
        cfg.numTeams = 3; cfg.teamMode = false;
        break;
    case 4:
        cfg.numTeams = 4; cfg.teamMode = false;
        break;
    }

    Game game;
    game.newGame(cfg);

    std::cout << FG_GREEN "Игра началась! " RST FG_GRAY "(q = выход)\n" RST;

    while (!game.isGameOver()) {
        Team currentTeam = game.currentTeam();
        int ti = static_cast<int>(currentTeam);

        // Check if current team is AI
        if (ti < MAX_TEAMS && isAI[ti]) {
            // AI turn
            auto moves = game.getLegalMoves();
            if (moves.empty()) { game.state().advanceTurn(); continue; }

            printBoard(game);
            printScores(game);
            std::cout << FG_CYAN BOLD "\n  [ИИ думает...]\n" RST;

            Move chosen = AI::chooseBestMove(game.state(), moves);
            std::cout << "  ИИ: " << moveDescription(chosen) << "\n";
            auto events = game.makeMove(chosen);
            printEvents(events);

            // Handle AI chain phases (arrow choice, horse, cave)
            int safety = 20;
            while (game.currentPhase() != TurnPhase::ChooseAction &&
                   game.currentPhase() != TurnPhase::TurnComplete && safety-- > 0) {
                auto phaseMoves = game.getLegalMoves();
                if (phaseMoves.empty()) break;
                Move phaseChoice = AI::chooseBestMove(game.state(), phaseMoves);
                std::cout << "  ИИ: " << moveDescription(phaseChoice) << "\n";
                auto phaseEvents = game.makeMove(phaseChoice);
                printEvents(phaseEvents);
            }

#ifndef _WIN32
            // Small pause so human can read
            struct timespec ts = {0, 500000000}; // 0.5s
            nanosleep(&ts, nullptr);
#else
            Sleep(500);
#endif
            continue;
        }

        // Human turn
        printBoard(game);
        printScores(game);

        auto moves = game.getLegalMoves();
        if (moves.empty()) {
            std::cout << FG_RED "Нет ходов! Пропуск.\n" RST;
            game.state().advanceTurn();
            continue;
        }

        std::cout << "\nХоды:\n";
        for (int i = 0; i < static_cast<int>(moves.size()); i++) {
            std::cout << "  " FG_BRIGHT_GREEN << std::setw(2) << i << RST ". "
                      << moveDescription(moves[i]) << "\n";
        }

        int choice = readInt("\nХод [0-" + std::to_string(moves.size()-1) + "]: ",
                             0, static_cast<int>(moves.size()) - 1);

        auto events = game.makeMove(moves[choice]);
        printEvents(events);

        // Handle chain phases for human
        while (game.currentPhase() != TurnPhase::ChooseAction &&
               game.currentPhase() != TurnPhase::TurnComplete) {
            auto phaseMoves = game.getLegalMoves();
            if (phaseMoves.empty()) break;
            std::cout << "\nВыберите:\n";
            for (int i = 0; i < static_cast<int>(phaseMoves.size()); i++) {
                std::cout << "  " FG_BRIGHT_GREEN << i << RST ". "
                          << moveDescription(phaseMoves[i]) << "\n";
            }
            int pc = readInt("Выбор: ", 0, static_cast<int>(phaseMoves.size()) - 1);
            auto pe = game.makeMove(phaseMoves[pc]);
            printEvents(pe);
        }
    }

    // Game over
    printBoard(game);
    std::cout << "\n" BOLD FG_BRIGHT_YELLOW "========= ИГРА ОКОНЧЕНА! =========\n" RST;
    for (int t = 0; t < game.state().config.numTeams; t++) {
        std::cout << "  " << teamAnsi(static_cast<Team>(t)) << teamName(static_cast<Team>(t))
                  << RST ": " << game.state().scores[t] << " монет\n";
    }
    Team winner = game.getWinner();
    std::cout << "\n" BOLD << teamAnsi(winner) << "ПОБЕДА: " << teamName(winner) << "!" RST "\n\n";
    return 0;
}
