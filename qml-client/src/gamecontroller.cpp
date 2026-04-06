#include "gamecontroller.h"
#include "boardmodel.h"
#include <QDebug>

GameController::GameController(QObject* parent) : QObject(parent) {
    m_aiTimer.setSingleShot(true);
    m_aiTimer.setInterval(350);
    connect(&m_aiTimer, &QTimer::timeout, this, &GameController::processAITurn);
}

void GameController::newGame(int numPlayers, bool teamMode, bool vsAI) {
    m_aiTimer.stop();
    if (numPlayers < 2) {
        // Return to menu
        m_gameActive = false;
        clearSelection();
        emit gameChanged();
        return;
    }

    GameConfig cfg;
    cfg.numTeams = numPlayers;
    cfg.teamMode = teamMode;
    cfg.seed = 0;

    m_isAI = {false, false, false, false};
    if (vsAI) {
        for (int i = 1; i < numPlayers; i++)
            m_isAI[i] = true;
    }

    m_game.newGame(cfg);
    m_gameActive = true;
    clearSelection();
    m_legalMoves = m_game.getLegalMoves();
    m_moveLog.clear();
    addLog("=== Новая игра ===");
    updateAll();
    emit gameChanged();

    scheduleAIIfNeeded();
}

void GameController::quitToMenu() {
    m_aiTimer.stop();
    m_gameActive = false;
    clearSelection();
    m_moveLog.clear();
    emit gameChanged();
    emit logChanged();
}

void GameController::newSandbox(int tileTypeId, int dirBits, int value) {
    m_aiTimer.stop();
    GameConfig cfg;
    cfg.numTeams = 2;
    cfg.teamMode = false;
    cfg.seed = 0;
    cfg.sandbox = true;
    cfg.sandboxTile = static_cast<TileType>(tileTypeId);
    cfg.sandboxDirBits = static_cast<uint8_t>(dirBits);
    cfg.sandboxValue = value;

    m_isAI = {false, false, false, false};

    m_game.newGame(cfg);
    m_gameActive = true;
    clearSelection();
    m_legalMoves = m_game.getLegalMoves();
    m_moveLog.clear();
    addLog("=== Песочница: " + QString(tileTypeName(cfg.sandboxTile)) + " ===");
    updateAll();
    emit gameChanged();
}

QVariantList GameController::sandboxTileTypes() const {
    QVariantList list;
    struct Entry { int id; const char* name; int dir; int val; };
    Entry entries[] = {
        {(int)TileType::Arrow,         "Arrow (1 dir N)",     1 << DIR_N, 0},
        {(int)TileType::Arrow,         "Arrow (2 dir N+S)",   (1<<DIR_N)|(1<<DIR_S), 0},
        {(int)TileType::Arrow,         "Arrow (4 card +)",    DIRS_CARDINAL, 0},
        {(int)TileType::Arrow,         "Arrow (4 diag X)",    DIRS_DIAGONAL, 0},
        {(int)TileType::Horse,         "Horse (L-move)",      0, 0},
        {(int)TileType::Jungle,        "Jungle (2 turns)",    0, 0},
        {(int)TileType::Desert,        "Desert (3 turns)",    0, 0},
        {(int)TileType::Swamp,         "Swamp (4 turns)",     0, 0},
        {(int)TileType::Mountain,      "Mountain (5 turns)",  0, 0},
        {(int)TileType::Ice,           "Ice (repeat dir)",    0, 0},
        {(int)TileType::Trap,          "Trap",                0, 0},
        {(int)TileType::Crocodile,     "Crocodile (back)",    0, 0},
        {(int)TileType::Cannibal,      "Cannibal (death)",    0, 0},
        {(int)TileType::Fortress,      "Fortress (safe)",     0, 0},
        {(int)TileType::ResurrectFort, "Resurrect Fort",      0, 0},
        {(int)TileType::Treasure,      "Treasure (3 coins)",  0, 3},
        {(int)TileType::Galleon,       "Galleon (3 value)",   0, 0},
        {(int)TileType::Balloon,       "Balloon (to ship)",   0, 0},
        {(int)TileType::Cannon,        "Cannon (fire N)",     1 << DIR_N, 0},
        {(int)TileType::Cave,          "Cave (tunnel)",       0, 0},
        {(int)TileType::Rum,           "Rum (3 bottles)",     0, 3},
        {(int)TileType::RumBarrel,     "Rum Barrel (skip)",   0, 0},
        {(int)TileType::BenGunn,       "Ben Gunn",            0, 0},
        {(int)TileType::Missionary,    "Missionary",          0, 0},
        {(int)TileType::Friday,        "Friday",              0, 0},
        {(int)TileType::ThickJungle,   "Thick Jungle",        0, 0},
        {(int)TileType::Grass,         "Grass (swap order)",  0, 0},
        {(int)TileType::Earthquake,    "Earthquake",          0, 0},
        {(int)TileType::Airplane,      "Airplane",            0, 0},
        {(int)TileType::Lighthouse,    "Lighthouse",          0, 0},
        {(int)TileType::Caramba,       "Caramba",             0, 0},
    };
    for (auto& e : entries) {
        QVariantMap m;
        m["id"] = e.id;
        m["name"] = QString(e.name);
        m["dir"] = e.dir;
        m["val"] = e.val;
        list.append(m);
    }
    return list;
}

QString GameController::assetsPath() const {
    if (m_boardModel) return m_boardModel->assetsPath();
    return "";
}

bool GameController::isAITurn() const {
    if (!m_gameActive) return false;
    int ti = static_cast<int>(m_game.currentTeam());
    return ti >= 0 && ti < MAX_TEAMS && m_isAI[ti];
}

QString GameController::currentTeamName() const {
    if (!m_gameActive) return "";
    return QString::fromUtf8(teamName(m_game.currentTeam()));
}

QString GameController::currentTeamColor() const {
    if (!m_gameActive) return "#808080";
    return QString::fromUtf8(teamColor(m_game.currentTeam()));
}

int GameController::turnNumber() const {
    return m_gameActive ? m_game.turnNumber() : 0;
}

QString GameController::statusText() const {
    if (!m_gameActive) return "Начните новую игру";
    if (m_game.isGameOver())
        return QString("Победа: %1").arg(QString::fromUtf8(teamName(m_game.getWinner())));
    if (isAITurn()) return "ИИ думает...";

    // Check if any pirate is on active spinner — hint to user
    const auto& s = m_game.state();
    int ti = static_cast<int>(s.currentTeam());
    for (int i = 0; i < PIRATES_PER_TEAM; i++) {
        auto& p = s.pirates[ti][i];
        if (s.isOnActiveSpinner(p)) {
            int required = spinnerSteps(s.tileAt(p.pos).type);
            return QString("Пират #%1 на вертушке (%2/%3). Кликните на него.")
                .arg(i).arg(p.spinnerProgress).arg(required);
        }
    }

    if (m_selectedPirate.valid()) return "Выберите клетку для хода";
    return "Выберите пирата или корабль";
}

QString GameController::phaseText() const {
    if (!m_gameActive) return "";
    switch (m_game.currentPhase()) {
    case TurnPhase::ChooseArrowDirection: return "Выберите направление стрелки";
    case TurnPhase::ChooseHorseDest:      return "Выберите клетку для хода конём";
    case TurnPhase::ChooseCaveExit:       return "Выберите выход из пещеры";
    default: return "";
    }
}

QVariantList GameController::scores() const {
    QVariantList list;
    if (!m_gameActive) return list;
    const auto& s = m_game.state();
    for (int t = 0; t < s.config.numTeams; t++) {
        QVariantMap m;
        Team team = static_cast<Team>(t);
        m["team"] = QString::fromUtf8(teamName(team));
        m["color"] = QString::fromUtf8(teamColor(team));
        m["score"] = s.scores[t];
        m["alive"] = s.alivePirateCount(team);
        m["isAI"] = m_isAI[t];
        list.append(m);
    }
    return list;
}

QVariantList GameController::pirates() const {
    QVariantList list;
    if (!m_gameActive) return list;
    const auto& s = m_game.state();
    for (int t = 0; t < s.config.numTeams; t++) {
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            const auto& p = s.pirates[t][i];
            if (p.state != PirateState::OnBoard && p.state != PirateState::InTrap) continue;
            if (!p.pos.valid()) continue;
            QVariantMap m;
            m["row"] = p.pos.row;
            m["col"] = p.pos.col;
            m["team"] = t;
            m["index"] = i;
            m["color"] = QString::fromUtf8(teamColor(static_cast<Team>(t)));
            m["hasCoin"] = p.carryingCoin || p.carryingGalleon;
            m["trapped"] = (p.state == PirateState::InTrap);
            m["selected"] = (m_selectedPirate.team == static_cast<Team>(t) &&
                             m_selectedPirate.index == i);
            m["isCurrentTeam"] = (static_cast<Team>(t) == s.currentTeam());
            m["spinnerProgress"] = p.spinnerProgress;
            list.append(m);
        }
    }
    for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
        const auto& ch = s.characters[ci];
        if (!ch.discovered || !ch.alive) continue;
        if (ch.onShip) continue; // on ship — not rendered as board token
        if (!ch.pos.valid()) continue;
        QVariantMap m;
        m["row"] = ch.pos.row;
        m["col"] = ch.pos.col;
        m["team"] = static_cast<int>(ch.owner);
        m["index"] = 10 + ci;
        m["color"] = ch.type == CharacterType::BenGunn ? "#00aa00" :
                     ch.type == CharacterType::Missionary ? "#4040ff" : "#8B4513";
        m["hasCoin"] = ch.carryingCoin || ch.carryingGalleon;
        m["trapped"] = false;
        m["selected"] = (m_selectedPirate.team == ch.owner &&
                         m_selectedPirate.index == 100 + ci);
        m["isCurrentTeam"] = (ch.owner == s.currentTeam());
        m["spinnerProgress"] = 0;
        list.append(m);
    }
    return list;
}

QVariantList GameController::shipList() const {
    QVariantList list;
    if (!m_gameActive) return list;
    const auto& s = m_game.state();
    for (int t = 0; t < s.config.numTeams; t++) {
        QVariantMap m;
        m["row"] = s.ships[t].pos.row;
        m["col"] = s.ships[t].pos.col;
        m["team"] = t;
        m["color"] = QString::fromUtf8(teamColor(static_cast<Team>(t)));
        m["isCurrentTeam"] = (static_cast<Team>(t) == s.currentTeam());
        int n = 0;
        for (int i = 0; i < PIRATES_PER_TEAM; i++)
            if (s.pirates[t][i].state == PirateState::OnShip) n++;
        m["piratesOnBoard"] = n;
        m["isAI"] = m_isAI[t];
        list.append(m);
    }
    return list;
}

QVariantList GameController::validTargets() const {
    QVariantList list;
    for (auto& c : m_validTargetCoords) {
        QVariantMap m;
        m["row"] = c.row;
        m["col"] = c.col;
        list.append(m);
    }
    return list;
}

// ============================================================
// Interaction
// ============================================================

void GameController::cellClicked(int row, int col) {
    if (!m_gameActive || m_game.isGameOver() || isAITurn()) return;

    Coord clicked = {row, col};
    const auto& s = m_game.state();

    if (s.phase != TurnPhase::ChooseAction) {
        for (auto& c : m_validTargetCoords)
            if (c == clicked) { tryExecuteMove(clicked); return; }
        return;
    }

    if (m_selectedPirate.valid()) {
        for (auto& c : m_validTargetCoords)
            if (c == clicked) { tryExecuteMove(clicked); return; }
        auto here = s.piratesAt(clicked, s.currentTeam());
        for (auto& pid : here)
            if (pid == m_selectedPirate) { clearSelection(); updateAll(); return; }
    }

    auto here = s.piratesAt(clicked, s.currentTeam());
    if (!here.empty()) {
        // If this pirate is on an active spinner, auto-advance instead of selecting
        const auto& clickedPirate = s.pirateRef(here[0]);
        if (s.isOnActiveSpinner(clickedPirate)) {
            // Find and execute the AdvanceSpinner move
            for (auto& mv : m_legalMoves) {
                if (mv.type == MoveType::AdvanceSpinner && mv.pirateId == here[0]) {
                    Move mvCopy = mv;
                    auto events = m_game.makeMove(mv);
                    processEvents(events);
                    addLog(describeMoveResult(mvCopy, events));

                    int progress = clickedPirate.spinnerProgress;
                    int required = spinnerSteps(s.tileAt(clickedPirate.pos).type);
                    if (progress >= required)
                        emit showMessage(QString("Вертушка пройдена! Пират свободен."));
                    else
                        emit showMessage(QString("Вертушка: шаг %1/%2").arg(progress).arg(required));

                    clearSelection();
                    m_legalMoves = m_game.getLegalMoves();
                    updateAll();
                    if (!m_game.isGameOver()) scheduleAIIfNeeded();
                    return;
                }
            }
        }
        // Always SELECT the pirate first. PickupCoin appears as valid target (same tile).
        selectPirate(here[0]);
        return;
    }

    // Check characters at clicked position
    for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
        auto& ch = s.characters[ci];
        if (ch.owner != s.currentTeam() || !ch.discovered || !ch.alive) continue;
        if (ch.pos == clicked) {
            selectPirate({s.currentTeam(), 100 + ci});
            return;
        }
    }

    // Check ships
    for (int t = 0; t < s.config.numTeams; t++) {
        if (static_cast<Team>(t) != s.currentTeam()) continue;
        if (s.ships[t].pos == clicked) {
            for (int i = 0; i < PIRATES_PER_TEAM; i++)
                if (s.pirates[t][i].state == PirateState::OnShip)
                    { selectPirate(s.pirates[t][i].id); return; }
        }
    }

    clearSelection();
    updateAll();
}

void GameController::selectPirate(const PirateId& id) {
    m_selectedPirate = id;
    m_movesForSelected.clear();
    m_validTargetCoords.clear();
    for (auto& mv : m_legalMoves) {
        if (mv.pirateId == id) {
            m_movesForSelected.push_back(mv);
            if (mv.to.valid()) {
                bool dup = false;
                for (auto& c : m_validTargetCoords) if (c == mv.to) { dup = true; break; }
                if (!dup) m_validTargetCoords.push_back(mv.to);
            }
        }
    }
    updateAll();
}

void GameController::clearSelection() {
    m_selectedPirate = {};
    m_movesForSelected.clear();
    m_validTargetCoords.clear();
    emit selectionChanged();
}

void GameController::tryExecuteMove(Coord target) {
    Move* chosen = nullptr;
    if (m_game.state().phase != TurnPhase::ChooseAction) {
        m_legalMoves = m_game.getLegalMoves();
        for (auto& mv : m_legalMoves)
            if (mv.to == target) { chosen = &mv; break; }
    } else {
        for (auto& mv : m_movesForSelected)
            if (mv.to == target) { chosen = &mv; break; }
    }
    if (!chosen) return;

    Move moveCopy = *chosen;
    auto events = m_game.makeMove(*chosen);
    processEvents(events);
    addLog(describeMoveResult(moveCopy, events));
    clearSelection();
    m_legalMoves = m_game.getLegalMoves();

    if (m_game.state().phase != TurnPhase::ChooseAction) {
        m_validTargetCoords.clear();
        for (auto& mv : m_legalMoves) {
            if (mv.to.valid()) {
                bool dup = false;
                for (auto& c : m_validTargetCoords) if (c == mv.to) { dup = true; break; }
                if (!dup) m_validTargetCoords.push_back(mv.to);
            }
        }
    }

    updateAll();

    if (m_game.isGameOver()) {
        emit gameOver(QString::fromUtf8(teamName(m_game.getWinner())));
        return;
    }

    scheduleAIIfNeeded();
}

void GameController::moveShipLeft()  { doShipMove(-1); }
void GameController::moveShipRight() { doShipMove(+1); }

void GameController::doShipMove(int direction) {
    if (!m_gameActive || m_game.isGameOver() || isAITurn()) return;
    if (m_game.state().phase != TurnPhase::ChooseAction) return;

    for (auto& mv : m_legalMoves) {
        if (mv.type != MoveType::MoveShip) continue;
        int si = static_cast<int>(m_game.currentTeam());
        Coord from = m_game.state().ships[si].pos;
        int dr = mv.to.row - from.row;
        int dc = mv.to.col - from.col;
        int moveDir = 0;
        if (from.row == 0)       moveDir = dc;
        else if (from.row == 12) moveDir = -dc;
        else if (from.col == 0)  moveDir = -dr;
        else if (from.col == 12) moveDir = dr;

        if ((direction < 0 && moveDir < 0) || (direction > 0 && moveDir > 0)) {
            auto events = m_game.makeMove(mv);
            processEvents(events);
            clearSelection();
            m_legalMoves = m_game.getLegalMoves();
            updateAll();
            if (!m_game.isGameOver()) scheduleAIIfNeeded();
            return;
        }
    }
}

void GameController::cancelSelection() {
    clearSelection();
    updateAll();
}

// ============================================================
// AI
// ============================================================

void GameController::scheduleAIIfNeeded() {
    if (!m_gameActive || m_game.isGameOver()) return;
    int ti = static_cast<int>(m_game.currentTeam());
    if (ti >= 0 && ti < MAX_TEAMS && m_isAI[ti]) {
        m_aiTimer.start();
    }
}

void GameController::processAITurn() {
    if (!m_gameActive || m_game.isGameOver()) return;
    int ti = static_cast<int>(m_game.currentTeam());
    if (ti < 0 || ti >= MAX_TEAMS || !m_isAI[ti]) return;

    auto moves = m_game.getLegalMoves();
    if (moves.empty()) {
        m_game.state().advanceTurn();
        updateAll();
        scheduleAIIfNeeded();
        return;
    }

    Move chosen = AI::chooseBestMove(m_game.state(), moves);
    auto events = m_game.makeMove(chosen);
    processEvents(events);
    addLog(describeMoveResult(chosen, events));

    // Handle chain phases (arrow choice, horse, cave)
    int safety = 20;
    while (m_game.currentPhase() != TurnPhase::ChooseAction &&
           m_game.currentPhase() != TurnPhase::TurnComplete && safety-- > 0) {
        auto phaseMoves = m_game.getLegalMoves();
        if (phaseMoves.empty()) break;
        Move pc = AI::chooseBestMove(m_game.state(), phaseMoves);
        auto pe = m_game.makeMove(pc);
        processEvents(pe);
        addLog(describeMoveResult(pc, pe));
    }

    clearSelection();
    m_legalMoves = m_game.getLegalMoves();
    updateAll();

    if (m_game.isGameOver()) {
        emit gameOver(QString::fromUtf8(teamName(m_game.getWinner())));
        return;
    }

    scheduleAIIfNeeded();
}

// ============================================================

void GameController::processEvents(const EventList& events) {
    for (auto& ev : events) {
        switch (ev.type) {
        case EventType::PirateDied:
            emit showMessage(QString("%1: пират погиб!").arg(
                QString::fromUtf8(teamName(ev.pirateId.team)))); break;
        case EventType::CoinLoaded:
            emit showMessage(QString("Монета на корабль! (+%1)").arg(
                ev.value > 0 ? ev.value : 1)); break;
        case EventType::CharacterJoined:
            emit showMessage("Персонаж присоединился!"); break;
        case EventType::BalloonLiftoff:
            emit showMessage("Воздушный шар! Пират вернулся на корабль. Кликните корабль для высадки."); break;
        case EventType::CoinPickedUp:
            emit showMessage(QString("Монета подобрана! (+%1)").arg(ev.value > 0 ? ev.value : 1)); break;
        case EventType::PirateTrapped:
            emit showMessage("Ловушка! Ждите спасения товарищем."); break;
        default: break;
        }
    }
}

void GameController::addLog(const QString& msg) {
    m_moveLog.append(msg);
    if (m_moveLog.size() > 500) m_moveLog.removeFirst();
    emit logChanged();
}

QString GameController::describeMoveResult(const Move& move, const EventList& events) {
    QString team = QString::fromUtf8(teamName(move.pirateId.team));
    QString desc = QString("[%1] ").arg(team);

    switch (move.type) {
    case MoveType::MoveShip:
        desc += QString("Корабль -> (%1,%2)").arg(move.to.row).arg(move.to.col); break;
    case MoveType::DisembarkPirate:
        desc += QString("Высадка #%1 -> (%2,%3)").arg(move.pirateId.index).arg(move.to.row).arg(move.to.col); break;
    case MoveType::MovePirate:
        desc += QString("#%1 (%2,%3)->(%4,%5)").arg(move.pirateId.index)
            .arg(move.from.row).arg(move.from.col).arg(move.to.row).arg(move.to.col); break;
    case MoveType::BoardShip:
        desc += QString("#%1 -> корабль").arg(move.pirateId.index); break;
    case MoveType::AdvanceSpinner:
        desc += QString("#%1 вертушка").arg(move.pirateId.index); break;
    case MoveType::PickupCoin:
        desc += QString("#%1 подобрал монету").arg(move.pirateId.index); break;
    case MoveType::ChooseDirection:
        desc += QString("направление -> (%1,%2)").arg(move.to.row).arg(move.to.col); break;
    case MoveType::ChooseHorseDest:
        desc += QString("конь -> (%1,%2)").arg(move.to.row).arg(move.to.col); break;
    case MoveType::MoveCharacter:
        desc += QString("персонаж -> (%1,%2)").arg(move.to.row).arg(move.to.col); break;
    case MoveType::ResurrectPirate:
        desc += QString("воскрешение #%1").arg(move.pirateId.index); break;
    default:
        desc += QString("ход %1").arg(static_cast<int>(move.type)); break;
    }

    // Append key events
    for (auto& ev : events) {
        switch (ev.type) {
        case EventType::TileRevealed:
            desc += QString(" [%1]").arg(tileTypeName(ev.tileType)); break;
        case EventType::PirateAttacked:
            desc += " [атака!]"; break;
        case EventType::PirateDied:
            desc += " [СМЕРТЬ]"; break;
        case EventType::CoinPickedUp:
            desc += " [+монета]"; break;
        case EventType::CoinLoaded:
            desc += QString(" [на корабль +%1]").arg(ev.value > 0 ? ev.value : 1); break;
        case EventType::BalloonLiftoff:
            desc += " [шар->корабль]"; break;
        case EventType::CannonFired:
            desc += " [пушка!]"; break;
        case EventType::PirateTrapped:
            desc += " [ловушка]"; break;
        case EventType::SpinnerAdvanced:
            desc += QString(" [шаг %1]").arg(ev.value); break;
        default: break;
        }
    }
    return desc;
}

void GameController::updateAll() {
    if (m_boardModel)
        m_boardModel->update(m_game.state(), m_selectedPirate, m_validTargetCoords);
    emit turnChanged();
    emit statusChanged();
    emit scoresChanged();
    emit boardChanged();
    emit selectionChanged();
}
