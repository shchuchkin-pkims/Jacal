#include "gamecontroller.h"
#include <QUrl>
#include "boardmodel.h"
#include "networkclient.h"
#include "map_def.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QColor>

GameController::GameController(QObject* parent) : QObject(parent) {
    m_aiTimer.setSingleShot(true);
    m_aiTimer.setInterval(350);
    connect(&m_aiTimer, &QTimer::timeout, this, &GameController::processAITurn);
}

void GameController::setNetworkClient(NetworkClient* nc) {
    m_networkClient = nc;
    if (!nc) return;
    connect(nc, &NetworkClient::gameStarted, this, &GameController::onNetworkGameStarted);
    connect(nc, &NetworkClient::gameMoveReceived, this, &GameController::onNetworkMoveReceived);
    connect(nc, &NetworkClient::gameOver, this, &GameController::onNetworkGameOver);
}

void GameController::onNetworkGameStarted(int seed, int numTeams, bool teamMode) {
    m_isNetworkGame = true;
    m_myNetworkTeam = m_networkClient->mySlot();
    for (int i = 0; i < MAX_TEAMS; i++) { m_isAI[i] = false; m_netSlotIsAI[i] = false; }

    // Determine which teams are AI from lobby slot data
    QVariantList slotList = m_networkClient->roomSlots();
    int teamIdx = 0;
    for (int i = 0; i < slotList.size() && teamIdx < MAX_TEAMS; i++) {
        QVariantMap slotMap = slotList[i].toMap();
        QString state = slotMap.value("state").toString();
        if (state == "closed") continue;
        if (state == "ai") m_netSlotIsAI[teamIdx] = true;
        teamIdx++;
    }

    // Copy game state from network client
    m_game = Game();
    m_game.state() = m_networkClient->game().state();

    m_gameActive = true;
    clearSelection();
    m_legalMoves = m_game.getLegalMoves();
    m_moveLog.clear();
    addLog("=== Сетевая игра ===");
    addLog(QString("Вы играете за %1 (слот %2)")
        .arg(QString::fromUtf8(teamName(static_cast<Team>(m_myNetworkTeam))))
        .arg(m_myNetworkTeam + 1));

    // Log slot/AI mapping
    for (int i = 0; i < numTeams; i++) {
        addLog(QString("  Команда %1: %2")
            .arg(QString::fromUtf8(teamName(static_cast<Team>(i))))
            .arg(m_netSlotIsAI[i] ? "ИИ" : "Игрок"));
    }

    updateAll();
    emit gameChanged();
}

void GameController::onNetworkMoveReceived(QJsonObject moveData) {
    if (!m_isNetworkGame || !m_gameActive) return;

    // NetworkClient already applied the move to its m_game.
    // Copy the authoritative state.
    m_game.state() = m_networkClient->game().state();

    clearSelection();
    m_legalMoves = m_game.getLegalMoves();

    TurnPhase phase = m_game.state().phase;
    int curTeam = static_cast<int>(m_game.currentTeam());

    // Log the move with detailed info
    QJsonObject moveObj = moveData["move"].toObject();
    int moveType = moveObj["type"].toInt();
    int pt = moveObj["pt"].toInt();
    int toR = moveObj["toR"].toInt(-1), toC = moveObj["toC"].toInt(-1);
    int fromR = moveObj["fromR"].toInt(-1), fromC = moveObj["fromC"].toInt(-1);
    addLog(QString("[%1] (%2,%3)->(%4,%5) тип=%6 фаза=%7 очередь=%8")
        .arg(QString::fromUtf8(teamName(static_cast<Team>(pt))))
        .arg(fromR).arg(fromC).arg(toR).arg(toC)
        .arg(moveType)
        .arg(static_cast<int>(phase))
        .arg(curTeam));

    // Handle phase-specific targets (arrow choice, horse, cave, etc.)
    if (phase != TurnPhase::ChooseAction) {
        // Show valid targets if this client can act (not AI)
        bool canAct = (curTeam >= 0 && curTeam < MAX_TEAMS && !m_netSlotIsAI[curTeam]);
        if (canAct) {
            m_validTargetCoords.clear();
            for (auto& mv : m_legalMoves) {
                if (mv.to.valid()) {
                    bool dup = false;
                    for (auto& c : m_validTargetCoords) if (c == mv.to) { dup = true; break; }
                    if (!dup) m_validTargetCoords.push_back(mv.to);
                }
            }
            addLog(QString("  -> Выбор phase=%1 targets=%2 moves=%3 mySlot=%4 curTeam=%5")
                .arg(static_cast<int>(phase))
                .arg(static_cast<int>(m_validTargetCoords.size()))
                .arg(static_cast<int>(m_legalMoves.size()))
                .arg(m_myNetworkTeam).arg(curTeam));
        } else {
            addLog(QString("  -> Ожидаю (AI или другой игрок, team=%1)").arg(curTeam));
        }
    }

    updateAll();

    if (m_game.isGameOver()) {
        emit gameOver(QString::fromUtf8(teamName(m_game.getWinner())));
    }
}

void GameController::onNetworkGameOver(QString winnerName) {
    addLog("=== Игра окончена: " + winnerName + " ===");
    emit gameOver(winnerName);
}

void GameController::newGameWithDensity(int numPlayers, bool teamMode, bool vsAI,
                                        const QString& mapId, float density) {
    m_pendingDensity = density;
    newGame(numPlayers, teamMode, vsAI, mapId);
}

void GameController::newGame(int numPlayers, bool teamMode, bool vsAI, const QString& mapId) {
    m_aiTimer.stop();
    if (numPlayers < 2) {
        m_gameActive = false;
        clearSelection();
        emit gameChanged();
        return;
    }

    GameConfig cfg;
    cfg.numTeams = numPlayers;
    cfg.teamMode = teamMode;
    cfg.seed = 0;
    cfg.mapId = mapId.toStdString();
    cfg.tileDensity = m_pendingDensity;
    m_pendingDensity = -1.0f; // reset

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
    addLog("=== " + mapId + " ===");
    updateAll();
    emit gameChanged();

    scheduleAIIfNeeded();
}

QString GameController::portraitsPath() const {
    if (!m_boardModel) return "";
    QString ap = m_boardModel->assetsPath();
    if (ap.isEmpty()) return "";
    // assets/tiles -> assets/portraits
    QDir d(ap + "/../portraits");
    return d.exists() ? d.absolutePath() : "";
}

int GameController::currentTeamRum() const {
    if (!m_gameActive) return 0;
    int ti = static_cast<int>(m_game.state().currentTeam());
    return (ti >= 0 && ti < MAX_TEAMS) ? m_game.state().rumOwned[ti] : 0;
}

QVariantList GameController::crewStatus() const {
    QVariantList list;
    if (!m_gameActive) return list;
    const auto& s = m_game.state();
    Team cur = s.currentTeam();

    // Current team's pirates first
    auto addPirates = [&](int t, bool isCurrentTeam) {
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            const auto& p = s.pirates[t][i];
            QVariantMap m;
            m["name"] = QString::fromUtf8(p.name.c_str());
            m["portrait"] = QString::fromUtf8(p.portrait.c_str());
            m["team"] = t;
            m["index"] = i;
            m["teamColor"] = QString::fromUtf8(teamColor(static_cast<Team>(t)));
            m["isCurrentTeam"] = isCurrentTeam;
            m["row"] = p.pos.row;
            m["col"] = p.pos.col;
            m["hasCoin"] = p.carryingCoin || p.carryingGalleon;
            m["isGalleon"] = p.carryingGalleon;
            QString status;
            QString xy = QString("(%1,%2)").arg(p.pos.col).arg(p.pos.row); // (x,y) format
            switch (p.state) {
                case PirateState::OnShip: status = "На корабле"; break;
                case PirateState::OnBoard: {
                    // Extended status based on tile type (Improvement 6)
                    if (s.mapIsLand(p.pos)) {
                        auto& tile = s.tileAt(p.pos);
                        if (tile.revealed) {
                            if (tile.type == TileType::Trap) status = "В ловушке! " + xy;
                            else if (tile.type == TileType::RumBarrel) status = "Пьёт ром " + xy;
                            else if (tile.type == TileType::Fortress || tile.type == TileType::ResurrectFort) status = "В крепости " + xy;
                            else if (tile.type == TileType::ThickJungle) status = "В джунглях " + xy;
                            else if (isSpinner(tile.type) && p.spinnerProgress > 0) {
                                int req = spinnerSteps(tile.type);
                                status = xy + QString(" [%1/%2]").arg(p.spinnerProgress).arg(req);
                            } else {
                                status = xy;
                            }
                        } else {
                            status = xy;
                        }
                    } else {
                        status = xy;
                    }
                    break;
                }
                case PirateState::Dead: status = "Погиб"; break;
                case PirateState::InTrap: status = "В ловушке! " + xy; break;
                case PirateState::InCave: status = "В пещере"; break;
            }
            m["status"] = status;
            m["state"] = static_cast<int>(p.state);
            m["isCharacter"] = false;
            m["selected"] = (m_selectedPirate.team == static_cast<Team>(t) && m_selectedPirate.index == i);
            m["teamIdx"] = t;
            m["unitIndex"] = i;
            // Check if this dead pirate can be resurrected (ResurrectPirate move exists)
            bool canRes = false;
            if (p.state == PirateState::Dead && isCurrentTeam) {
                for (auto& mv : m_legalMoves) {
                    if (mv.type == MoveType::ResurrectPirate && mv.pirateId.index == i) {
                        canRes = true; break;
                    }
                }
            }
            m["canResurrect"] = canRes;
            list.append(m);
        }
    };

    // Current team
    addPirates(static_cast<int>(cur), true);

    // Characters owned by current team
    for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
        const auto& ch = s.characters[ci];
        if (ch.owner != cur || !ch.discovered) continue;
        QVariantMap m;
        const char* charNames[] = {"Бен Ганн", "Бен Ганн", "Бен Ганн", "Миссионер", "Пятница"};
        const char* charPortraits[] = {"ben_gann.png", "ben_gann.png", "ben_gann.png",
                                       "missioner.png", "friday.png"};
        // Improvement 1: converted Missionary gets different name/portrait
        QString cname = charNames[ci];
        QString cportrait = charPortraits[ci];
        if (ch.type == CharacterType::Missionary && ch.convertedToPirate) {
            cname = "Миссионер (пират)";
            cportrait = "missioner_drunk.png";
        }
        m["name"] = cname;
        m["portrait"] = cportrait;
        m["team"] = static_cast<int>(ch.owner);
        m["index"] = 100 + ci;
        m["teamColor"] = QString::fromUtf8(teamColor(ch.owner));
        m["isCurrentTeam"] = true;
        m["row"] = ch.pos.row;
        m["col"] = ch.pos.col;
        m["hasCoin"] = ch.carryingCoin || ch.carryingGalleon;
        m["isGalleon"] = ch.carryingGalleon;
        m["status"] = ch.alive ? QString("(%1,%2)").arg(ch.pos.col).arg(ch.pos.row) : "Погиб";
        m["state"] = ch.alive ? 1 : 2;
        m["isCharacter"] = true;
        m["selected"] = (m_selectedPirate.team == ch.owner && m_selectedPirate.index == 100 + ci);
        m["teamIdx"] = static_cast<int>(ch.owner);
        m["unitIndex"] = 100 + ci;
        list.append(m);
    }

    // Other teams (smaller)
    for (int t = 0; t < s.config.numTeams; t++) {
        if (static_cast<Team>(t) == cur) continue;
        addPirates(t, false);
    }

    return list;
}

void GameController::selectCrewMember(int team, int index) {
    if (!m_gameActive || m_game.isGameOver() || isAITurn()) return;
    if (static_cast<Team>(team) != m_game.currentTeam()) return;
    PirateId id = {static_cast<Team>(team), index};
    selectPirate(id);
}

void GameController::quitToMenu() {
    m_aiTimer.stop();
    m_gameActive = false;
    m_isNetworkGame = false;
    m_myNetworkTeam = -1;
    clearSelection();
    m_moveLog.clear();
    if (m_server) { m_server->stop(); delete m_server; m_server = nullptr; }
    if (m_networkClient && m_networkClient->isConnected()) m_networkClient->disconnect();
    emit gameChanged();
    emit logChanged();
    emit screenChanged("main");
}

void GameController::hostGame(const QString& roomName, int port) {
    if (m_server) { m_server->stop(); delete m_server; }
    m_server = new GameServer(this);
    m_server->setRoomName(roomName);
    if (!m_server->start(static_cast<quint16>(port))) {
        emit showMessage("Не удалось запустить сервер");
        delete m_server; m_server = nullptr;
        return;
    }
    emit showMessage("Сервер запущен на порту " + QString::number(m_server->port()));
    // The QML NetworkClient will connect to localhost
}

void GameController::showNetworkScreen() {
    emit screenChanged("network");
}

void GameController::showMainMenu() {
    if (m_server) { m_server->stop(); delete m_server; m_server = nullptr; }
    emit screenChanged("main");
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

QVariantList GameController::availableMaps() const {
    QVariantList list;
    // Built-in maps
    auto& maps = getBuiltinMaps();
    for (auto& m : maps) {
        QVariantMap entry;
        entry["id"] = QString::fromStdString(m.id);
        entry["name"] = QString::fromStdString(m.name);
        entry["minPlayers"] = m.minPlayers;
        entry["maxPlayers"] = m.maxPlayers;
        entry["landCells"] = m.countLandCells();
        list.append(entry);
    }
    // Custom maps from maps/ folder
    if (m_boardModel) {
        QString ap = m_boardModel->assetsPath();
        QDir mapsDir(ap + "/../../maps");
        if (mapsDir.exists()) {
            auto customs = loadCustomMaps(mapsDir.absolutePath().toStdString());
            registerCustomMaps(customs); // make findable by findMap()
            for (auto& m : customs) {
                QVariantMap entry;
                entry["id"] = QString::fromStdString(m.id);
                entry["name"] = QString::fromStdString(m.name) + " *";
                entry["minPlayers"] = m.minPlayers;
                entry["maxPlayers"] = m.maxPlayers;
                entry["landCells"] = m.countLandCells();
                list.append(entry);
            }
        }
    }
    return list;
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



QString GameController::fileUrl(const QString& relativePath) const {
    if (!m_boardModel) return QString();
    QString full = m_boardModel->assetsPath() + "/" + relativePath;
    return QUrl::fromLocalFile(full).toString();
}
QString GameController::assetsPath() const {
    if (m_boardModel) return m_boardModel->assetsPath();
    return "";
}

QString GameController::mapPreviewUrl(const QString& mapId) const {
    if (!m_boardModel) return "";
    QString ap = m_boardModel->assetsPath();
    // Maps folder: assets/tiles/../../maps/ = project_root/maps/
    QDir mapsDir(ap + "/../../maps");
    QString pngPath = mapsDir.absoluteFilePath(mapId + ".png");
    if (QFile::exists(pngPath))
        return QUrl::fromLocalFile(pngPath).toString();
    return "";
}

bool GameController::isAITurn() const {
    if (!m_gameActive) return false;
    if (m_isNetworkGame) {
        // In network game, block input only for AI-controlled teams.
        // Human players (even opponents on the same client) can click —
        // the server validates and rejects unauthorized moves.
        int curTeam = static_cast<int>(m_game.currentTeam());
        if (curTeam >= 0 && curTeam < MAX_TEAMS && m_netSlotIsAI[curTeam])
            return true;
        return false;
    }
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
    if (m_rumUseMode) return "Выберите цель для рома (кликните пирата или персонажа)";

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
    case TurnPhase::ChooseAirplaneTarget: return "Самолёт! Выберите куда лететь (любая клетка)";
    case TurnPhase::ChooseLighthouseTiles: {
        int rem = m_game.state().lighthouseRemaining;
        return QString("Маяк! Выберите клетку для разведки (осталось: %1)").arg(rem);
    }
    case TurnPhase::ChooseEarthquakeTiles: {
        if (!m_game.state().earthquakeFirst.valid())
            return "Землетрясение! Выберите 1-ю клетку для обмена";
        return "Землетрясение! Выберите 2-ю клетку для обмена";
    }
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
            m["portrait"] = QString::fromUtf8(p.portrait.c_str());
            m["isCharacter"] = false;
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
        const char* charPortraits[] = {"ben_gann.png", "ben_gann.png", "ben_gann.png",
                                       "missioner.png", "friday.png"};
        QString cport = charPortraits[ci];
        if (ch.type == CharacterType::Missionary && ch.convertedToPirate)
            cport = "missioner_drunk.png";
        m["portrait"] = cport;
        m["isCharacter"] = true;
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

    // Rum use mode: find matching UseRum move for clicked position
    if (m_rumUseMode) {
        for (auto& mv : m_rumMoves) {
            Coord target = (mv.characterIndex >= 0) ? mv.to : mv.from;
            if (target == clicked) {
                auto events = m_game.makeMove(mv);
                processEvents(events);
                addLog(describeMoveResult(mv, events));
                m_rumUseMode = false;
                m_rumMoves.clear();
                clearSelection();
                m_legalMoves = m_game.getLegalMoves();
                updateAll();
                if (!m_game.isGameOver()) scheduleAIIfNeeded();
                return;
            }
        }
        // Clicked non-target → cancel rum mode
        cancelRumUse();
        return;
    }

    if (m_selectedPirate.valid()) {
        // If clicked is a valid move target -> execute
        for (auto& c : m_validTargetCoords)
            if (c == clicked) { tryExecuteMove(clicked); return; }
        // If clicked on same tile as selected pirate -> cycle to next unit (dont deselect)
        // Deselection only if clicking on empty tile or different location
    }

    // Check if clicking on a trapped pirate → auto use rum if available
    {
        int ti = static_cast<int>(s.currentTeam());
        for (int i = 0; i < PIRATES_PER_TEAM; i++) {
            auto& tp = s.pirates[ti][i];
            if (tp.pos == clicked && (tp.state == PirateState::InTrap || tp.state == PirateState::InCave)) {
                for (auto& mv : m_legalMoves) {
                    if (mv.type == MoveType::UseRum && mv.pirateId == tp.id && mv.characterIndex < 0) {
                        Move mvCopy = mv;
                        auto events = m_game.makeMove(mv);
                        processEvents(events);
                        addLog(describeMoveResult(mvCopy, events));
                        clearSelection();
                        m_legalMoves = m_game.getLegalMoves();
                        updateAll();
                        if (!m_game.isGameOver()) scheduleAIIfNeeded();
                        return;
                    }
                }
            }
        }
    }

    // Collect ALL selectable units at this position (pirates + characters)
    std::vector<PirateId> unitsHere;
    {
        auto pirates = s.piratesAt(clicked, s.currentTeam());
        for (auto& pid : pirates) unitsHere.push_back(pid);
        for (int ci = 0; ci < MAX_CHARACTERS; ci++) {
            auto& ch = s.characters[ci];
            if (ch.owner == s.currentTeam() && ch.discovered && ch.alive && ch.pos == clicked)
                unitsHere.push_back({s.currentTeam(), 100 + ci});
        }
    }

    if (!unitsHere.empty()) {
        // Check spinner auto-advance (only for regular pirates)
        if (unitsHere.size() == 1 && unitsHere[0].index < PIRATES_PER_TEAM) {
            const auto& cp = s.pirateRef(unitsHere[0]);
            if (s.isOnActiveSpinner(cp)) {
                for (auto& mv : m_legalMoves) {
                    if (mv.type == MoveType::AdvanceSpinner && mv.pirateId == unitsHere[0]) {
                        if (m_isNetworkGame && m_networkClient) {
                            m_networkClient->sendMove(Protocol::moveToJson(mv));
                            return;
                        }
                        Move mvCopy = mv;
                        auto events = m_game.makeMove(mv);
                        processEvents(events);
                        addLog(describeMoveResult(mvCopy, events));
                        int prog = cp.spinnerProgress;
                        int req = spinnerSteps(s.tileAt(cp.pos).type);
                        emit showMessage(prog >= req ?
                            QString("Вертушка пройдена!") :
                            QString("Вертушка: шаг %1/%2").arg(prog).arg(req));
                        clearSelection();
                        m_legalMoves = m_game.getLegalMoves();
                        updateAll();
                        if (!m_game.isGameOver()) scheduleAIIfNeeded();
                        return;
                    }
                }
            }
        }

        // Cycle through units: if currently selected unit is in this list,
        // pick the NEXT one. Otherwise pick the first.
        PirateId toSelect = unitsHere[0];
        if (m_selectedPirate.valid()) {
            for (size_t i = 0; i < unitsHere.size(); i++) {
                if (unitsHere[i] == m_selectedPirate) {
                    toSelect = unitsHere[(i + 1) % unitsHere.size()];
                    break;
                }
            }
        }
        selectPirate(toSelect);
        return;
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

    // === NETWORK GAME: send to server, DON'T apply locally ===
    // Server will validate and broadcast back to ALL clients (including us).
    // We apply the move only when we receive game_move from the server.
    if (m_isNetworkGame && m_networkClient) {
        m_networkClient->sendMove(Protocol::moveToJson(moveCopy));
        clearSelection();
        updateAll();
        return;
    }

    // === LOCAL GAME: apply immediately ===
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
            if (m_isNetworkGame && m_networkClient) {
                m_networkClient->sendMove(Protocol::moveToJson(mv));
                return;
            }
            auto events = m_game.makeMove(mv);
            processEvents(events);
            addLog(describeMoveResult(mv, events));
            clearSelection();
            m_legalMoves = m_game.getLegalMoves();
            updateAll();
            if (!m_game.isGameOver()) scheduleAIIfNeeded();
            return;
        }
    }
}

void GameController::cancelSelection() {
    m_rumUseMode = false;
    m_rumMoves.clear();
    clearSelection();
    updateAll();
}

void GameController::activateRumUse() {
    if (!m_gameActive || m_game.isGameOver() || isAITurn()) return;
    int ti = static_cast<int>(m_game.currentTeam());
    if (m_game.state().rumOwned[ti] <= 0) {
        emit showMessage("Нет бутылок рома!");
        return;
    }

    // Collect all UseRum moves
    m_rumMoves.clear();
    m_validTargetCoords.clear();
    for (auto& mv : m_legalMoves) {
        if (mv.type == MoveType::UseRum) {
            m_rumMoves.push_back(mv);
            if (mv.to.valid()) {
                bool dup = false;
                for (auto& c : m_validTargetCoords) if (c == mv.to) { dup = true; break; }
                if (!dup) m_validTargetCoords.push_back(mv.to);
            }
            // For freeing trapped pirates, target is the pirate's position
            if (mv.from.valid() && mv.characterIndex < 0) {
                bool dup = false;
                for (auto& c : m_validTargetCoords) if (c == mv.from) { dup = true; break; }
                if (!dup) m_validTargetCoords.push_back(mv.from);
            }
        }
    }

    if (m_rumMoves.empty()) {
        emit showMessage("Нет целей для рома");
        return;
    }

    m_rumUseMode = true;
    m_selectedPirate = {}; // clear pirate selection
    emit showMessage("Выберите цель для рома (пират/персонаж)");
    updateAll();
}

void GameController::cancelRumUse() {
    m_rumUseMode = false;
    m_rumMoves.clear();
    m_validTargetCoords.clear();
    updateAll();
}

void GameController::resurrectPirate(int pirateIndex) {
    if (!m_gameActive || m_game.isGameOver() || isAITurn()) return;

    // Find the ResurrectPirate move for this pirate
    for (auto& mv : m_legalMoves) {
        if (mv.type == MoveType::ResurrectPirate && mv.pirateId.index == pirateIndex) {
            auto events = m_game.makeMove(mv);
            processEvents(events);
            addLog(describeMoveResult(mv, events));
            clearSelection();
            m_legalMoves = m_game.getLegalMoves();
            updateAll();
            emit showMessage("Пират воскрешён!");
            if (!m_game.isGameOver()) scheduleAIIfNeeded();
            return;
        }
    }
    emit showMessage("Невозможно воскресить");
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
        case EventType::RumUsed:
            emit showMessage("Ром использован!"); break;
        case EventType::CharacterDied:
            emit showMessage("Персонаж погиб!"); break;
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
        desc += QString("Корабль -> (%1,%2)").arg(move.to.col).arg(move.to.row); break;
    case MoveType::DisembarkPirate:
        desc += QString("Высадка #%1 -> (%2,%3)").arg(move.pirateId.index).arg(move.to.col).arg(move.to.row); break;
    case MoveType::MovePirate:
        desc += QString("#%1 (%2,%3)->(%4,%5)").arg(move.pirateId.index)
            .arg(move.from.col).arg(move.from.row).arg(move.to.col).arg(move.to.row); break;
    case MoveType::BoardShip:
        desc += QString("#%1 -> корабль").arg(move.pirateId.index); break;
    case MoveType::AdvanceSpinner:
        desc += QString("#%1 вертушка").arg(move.pirateId.index); break;
    case MoveType::PickupCoin:
        desc += QString("#%1 подобрал монету").arg(move.pirateId.index); break;
    case MoveType::ChooseDirection:
        desc += QString("направление -> (%1,%2)").arg(move.to.col).arg(move.to.row); break;
    case MoveType::ChooseHorseDest:
        desc += QString("конь -> (%1,%2)").arg(move.to.col).arg(move.to.row); break;
    case MoveType::MoveCharacter:
        desc += QString("персонаж -> (%1,%2)").arg(move.to.col).arg(move.to.row); break;
    case MoveType::ResurrectPirate:
        desc += QString("воскрешение #%1").arg(move.pirateId.index); break;
    case MoveType::UseRum:
        if (move.characterIndex >= 0)
            desc += QString("ром -> персонаж (%1,%2)").arg(move.to.col).arg(move.to.row);
        else
            desc += QString("ром -> освободить #%1").arg(move.pirateId.index);
        break;
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

void GameController::generateMapPreview(const MapDefinition& md, const QString& pngPath) {
    int cpx = 16;
    int w = BOARD_SIZE * cpx, h = BOARD_SIZE * cpx;
    QImage img(w, h, QImage::Format_RGB888);

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int r = py / cpx, c = px / cpx;
            QColor col;

            // Ship spawn?
            bool ship = false; int st = -1;
            for (auto& sp : md.ships) {
                if (sp.row == r && sp.col == c) { ship = true; st = sp.team; break; }
            }

            if (ship) {
                switch(st) {
                    case 0: col = QColor(220,220,220); break;
                    case 1: col = QColor(240,208,0);   break;
                    case 2: col = QColor(60,60,60);    break;
                    case 3: col = QColor(208,48,48);   break;
                    default: col = QColor(128,128,128); break;
                }
            } else {
                switch(md.terrain[r][c]) {
                    case Terrain::Sea:  col = QColor(26,96,144);  break;
                    case Terrain::Land: col = QColor(45,90,30);   break;
                    case Terrain::Rock: col = QColor(106,90,74);  break;
                }
            }

            // Grid lines (darker)
            if (px % cpx == 0 || py % cpx == 0)
                col = col.darker(130);

            img.setPixelColor(px, py, col);
        }
    }

    img.save(pngPath, "PNG");
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

void GameController::showMapEditor() {
    emit screenChanged("editor");
}

void GameController::saveCustomMap(const QString& name, const QString& terrain, const QString& ships) {
    MapDefinition md;
    md.name = name.toStdString();
    // Generate id from name (lowercase, replace spaces)
    std::string id = name.toStdString();
    for (auto& c : id) { if (c == ' ') c = '_'; c = tolower(c); }
    md.id = "custom_" + id;

    // Parse terrain string (rows separated by \n)
    QStringList rows = terrain.split('\n', Qt::SkipEmptyParts);
    for (int r = 0; r < BOARD_SIZE && r < rows.size(); r++) {
        for (int c = 0; c < BOARD_SIZE && c < rows[r].size(); c++) {
            QChar ch = rows[r][c];
            if (ch == '#') md.terrain[r][c] = Terrain::Land;
            else if (ch == 'X') md.terrain[r][c] = Terrain::Rock;
            else md.terrain[r][c] = Terrain::Sea;
        }
    }

    // Parse ships string (team,row,col;team,row,col;...)
    QStringList shipParts = ships.split(';', Qt::SkipEmptyParts);
    for (auto& sp : shipParts) {
        QStringList vals = sp.split(',');
        if (vals.size() >= 3) {
            ShipSpawn ss;
            ss.team = vals[0].toInt();
            ss.row = vals[1].toInt();
            ss.col = vals[2].toInt();
            md.ships.push_back(ss);
        }
    }

    md.minPlayers = 2;
    md.maxPlayers = static_cast<int>(md.ships.size());
    if (md.maxPlayers < 2) md.maxPlayers = 2;

    // Save to maps directory
    QString mapsDir;
    if (m_boardModel) {
        QString ap = m_boardModel->assetsPath();
        QDir d(ap + "/../../maps");
        if (!d.exists()) d.mkpath(".");
        mapsDir = d.absolutePath();
    } else {
        mapsDir = QDir::homePath() + "/Documents/VS_Code/Jacal/maps";
        QDir(mapsDir).mkpath(".");
    }

    QString filepath = mapsDir + "/" + QString::fromStdString(md.id) + ".jmap";
    if (saveMapToFile(md, filepath.toStdString())) {
        // Generate PNG preview
        QString pngPath = mapsDir + "/" + QString::fromStdString(md.id) + ".png";
        generateMapPreview(md, pngPath);
        emit showMessage("Карта сохранена: " + filepath);
    } else {
        emit showMessage("Ошибка сохранения карты!");
    }
}
