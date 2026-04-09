#include "gameserver.h"
#include "map_def.h"
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonArray>
#include <random>

GameServer::GameServer(QObject* parent) : QObject(parent) {
    m_broadcastTimer.setInterval(2000);
    connect(&m_broadcastTimer, &QTimer::timeout, this, &GameServer::onBroadcastTimer);

    m_aiTimer.setInterval(500);
    m_aiTimer.setSingleShot(true);
    connect(&m_aiTimer, &QTimer::timeout, this, &GameServer::onAITimer);

    // Default: all slots open
    for (int i = 0; i < 4; i++)
        m_slots[i] = {Protocol::SlotState::Open, "", -1, false};
}

GameServer::~GameServer() { stop(); }

bool GameServer::start(quint16 port) {
    stop();
    m_port = port;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &GameServer::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        emit logMessage("Server failed to start: " + m_server->errorString());
        delete m_server; m_server = nullptr;
        return false;
    }

    // UDP broadcast for LAN discovery
    m_broadcastSocket = new QUdpSocket(this);
    m_broadcastTimer.start();

    m_inGame = false;
    m_chatLog.clear();
    for (int i = 0; i < 4; i++)
        m_slots[i] = {Protocol::SlotState::Open, "", -1, false};

    emit logMessage("Server started on port " + QString::number(port));
    return true;
}

void GameServer::stop() {
    m_broadcastTimer.stop();
    m_aiTimer.stop();
    if (m_broadcastSocket) { delete m_broadcastSocket; m_broadcastSocket = nullptr; }
    for (auto& ci : m_clients) {
        if (ci.socket) { ci.socket->disconnectFromHost(); ci.socket->deleteLater(); }
    }
    m_clients.clear();
    if (m_server) { m_server->close(); delete m_server; m_server = nullptr; }
    m_game.reset();
}

void GameServer::onBroadcastTimer() {
    if (!m_broadcastSocket || !m_server) return;
    // Count players
    int players = 0;
    for (int i = 0; i < 4; i++)
        if (m_slots[i].state == Protocol::SlotState::Player) players++;

    QJsonObject info;
    info["magic"] = Protocol::BROADCAST_MAGIC;
    info["name"] = m_roomName;
    info["port"] = m_port;
    info["players"] = players;
    info["maxPlayers"] = 4;
    info["inGame"] = m_inGame;
    info["version"] = Protocol::APP_VERSION;

    QByteArray data = QJsonDocument(info).toJson(QJsonDocument::Compact);
    m_broadcastSocket->writeDatagram(data, QHostAddress::Broadcast, Protocol::BROADCAST_PORT);
}

void GameServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        int id = m_nextClientId++;
        ClientInfo ci;
        ci.socket = socket;
        ci.name = "Player " + QString::number(id);
        ci.slot = -1;
        m_clients[id] = ci;

        connect(socket, &QTcpSocket::readyRead, this, &GameServer::onClientData);
        connect(socket, &QTcpSocket::disconnected, this, &GameServer::onClientDisconnected);

        socket->setProperty("clientId", id);

        // First client is host
        if (m_hostClientId < 0) m_hostClientId = id;

        // Auto-assign to first open slot
        for (int i = 0; i < 4; i++) {
            if (m_slots[i].state == Protocol::SlotState::Open) {
                m_slots[i].state = Protocol::SlotState::Player;
                m_slots[i].playerName = ci.name;
                m_slots[i].clientId = id;
                m_clients[id].slot = i;
                break;
            }
        }

        // Send welcome + room state
        QJsonObject welcome;
        welcome["clientId"] = id;
        welcome["isHost"] = (id == m_hostClientId);
        welcome["slots"] = slotsToJson();
        welcome["chat"] = chatToJson();
        welcome["roomName"] = m_roomName;
        welcome["inGame"] = m_inGame;
        sendTo(id, Protocol::makeMsg("welcome", welcome));

        broadcastRoomState();
        emit logMessage("Client " + QString::number(id) + " connected");
    }
}

void GameServer::onClientData() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    int clientId = socket->property("clientId").toInt();

    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonObject msg = Protocol::parseMsg(line);
        if (!msg.isEmpty()) handleMessage(clientId, msg);
    }
}

void GameServer::onClientDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    int clientId = socket->property("clientId").toInt();

    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        int slot = it->slot;
        if (slot >= 0 && slot < 4) {
            m_slots[slot].state = Protocol::SlotState::Open;
            m_slots[slot].playerName = "";
            m_slots[slot].clientId = -1;
            m_slots[slot].ready = false;
        }
        m_clients.erase(it);
    }
    socket->deleteLater();

    broadcastRoomState();
    emit logMessage("Client " + QString::number(clientId) + " disconnected");

    if (clientId == m_hostClientId) {
        // Transfer host to next client
        if (!m_clients.isEmpty())
            m_hostClientId = m_clients.firstKey();
        else
            m_hostClientId = -1;
    }
}

void GameServer::handleMessage(int clientId, const QJsonObject& msg) {
    QString type = msg["t"].toString();

    if (type == "set_name") {
        QString name = msg["name"].toString().left(24);
        m_clients[clientId].name = name;
        int slot = m_clients[clientId].slot;
        if (slot >= 0) m_slots[slot].playerName = name;
        broadcastRoomState();
    }
    else if (type == "chat") {
        QString text = msg["text"].toString().left(200);
        QString from = m_clients.contains(clientId) ? m_clients[clientId].name : "?";
        m_chatLog.append(from + ": " + text);
        if (m_chatLog.size() > 100) m_chatLog.removeFirst();

        QJsonObject chatMsg;
        chatMsg["from"] = from;
        chatMsg["text"] = text;
        sendToAllInRoom(Protocol::makeMsg("chat", chatMsg));
    }
    else if (type == "set_slot" && clientId == m_hostClientId && !m_inGame) {
        int slot = msg["slot"].toInt();
        QString state = msg["state"].toString();
        if (slot >= 0 && slot < 4 && m_slots[slot].state != Protocol::SlotState::Player) {
            m_slots[slot].state = Protocol::slotStateFromStr(state);
            m_slots[slot].playerName = (m_slots[slot].state == Protocol::SlotState::AI) ? "ИИ" : "";
            m_slots[slot].clientId = -1;
            broadcastRoomState();
        }
    }
    else if (type == "ready" && !m_inGame) {
        int slot = m_clients[clientId].slot;
        if (slot >= 0) {
            m_slots[slot].ready = !m_slots[slot].ready;
            broadcastRoomState();
        }
    }
    else if (type == "start_game" && clientId == m_hostClientId && !m_inGame) {
        // Check all slots filled (not Open)
        for (int i = 0; i < 4; i++) {
            if (m_slots[i].state == Protocol::SlotState::Open) {
                sendTo(clientId, Protocol::makeMsg("error",
                    {{"msg", "Все слоты должны быть заполнены"}}));
                return;
            }
        }

        // Determine game config
        // numTeams = highest active slot index + 1
        // This ensures direct slot→team mapping (slot 0=White, 1=Yellow, etc.)
        int numTeams = 0;
        for (int i = 0; i < 4; i++) {
            if (m_slots[i].state != Protocol::SlotState::Closed)
                numTeams = i + 1;
        }

        GameConfig cfg;
        cfg.numTeams = numTeams;
        cfg.teamMode = false;
        cfg.mapId = m_mapId;
        std::random_device rd;
        cfg.seed = rd();
        m_gameSeed = cfg.seed;

        m_game = std::make_unique<Game>();
        m_game->newGame(cfg);
        m_inGame = true;

        // Broadcast game start
        QJsonObject startMsg;
        startMsg["seed"] = static_cast<int>(m_gameSeed);
        startMsg["numTeams"] = numTeams;
        startMsg["teamMode"] = false;
        sendToAllInRoom(Protocol::makeMsg("game_started", startMsg));

        emit logMessage("Game started with " + QString::number(numTeams) + " teams");

        // If first team is AI, trigger AI
        processAITurns();
    }
    else if (type == "game_move" && m_inGame && m_game) {
        // Validate: is it this client's turn?
        Team currentTeam = m_game->currentTeam();
        int currentSlot = slotForTeam(currentTeam);
        if (currentSlot < 0 || m_slots[currentSlot].clientId != clientId) {
            sendTo(clientId, Protocol::makeMsg("error", {{"msg", "Не ваш ход"}}));
            return;
        }

        Move move = Protocol::moveFromJson(msg["move"].toObject());

        // Validate move is legal
        auto legal = m_game->getLegalMoves();
        bool isLegal = false;
        for (auto& lm : legal) {
            if (lm.type == move.type && lm.to == move.to && lm.pirateId == move.pirateId) {
                isLegal = true;
                move = lm; // use the full legal move (has all fields)
                break;
            }
        }
        if (!isLegal) {
            sendTo(clientId, Protocol::makeMsg("error", {{"msg", "Недопустимый ход"}}));
            return;
        }

        processGameMove(clientId, move);
    }
}

void GameServer::processGameMove(int clientId, const Move& move) {
    auto events = m_game->makeMove(move);

    // If the move resulted in a choice phase (arrow, horse, cave) and the current
    // team is AI — resolve it immediately BEFORE broadcasting, so clients never
    // see the intermediate phase for AI players.
    int safety = 20;
    while (m_game && m_game->currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
        Team phaseTeam = m_game->currentTeam();
        int phaseSlot = slotForTeam(phaseTeam);
        if (phaseSlot >= 0 && isSlotAI(phaseSlot)) {
            // AI resolves the choice instantly
            auto phaseMoves = m_game->getLegalMoves();
            if (phaseMoves.empty()) break;
            Move pc = AI::chooseBestMove(m_game->state(), phaseMoves);
            auto phaseEvents = m_game->makeMove(pc);
            // Merge events
            events.insert(events.end(), phaseEvents.begin(), phaseEvents.end());
        } else {
            // Human player needs to choose — stop here, broadcast current state
            break;
        }
    }

    // Broadcast the move (with all AI chain resolutions included)
    QJsonObject moveMsg;
    moveMsg["move"] = Protocol::moveToJson(move);
    QJsonArray evArr;
    for (auto& ev : events) {
        QJsonObject e;
        e["type"] = static_cast<int>(ev.type);
        e["r1"] = ev.pos.row; e["c1"] = ev.pos.col;
        e["r2"] = ev.pos2.row; e["c2"] = ev.pos2.col;
        e["pt"] = static_cast<int>(ev.pirateId.team);
        e["pi"] = ev.pirateId.index;
        e["tt"] = static_cast<int>(ev.tileType);
        e["val"] = ev.value;
        evArr.append(e);
    }
    moveMsg["events"] = evArr;
    moveMsg["turn"] = m_game->turnNumber();
    moveMsg["currentTeam"] = static_cast<int>(m_game->currentTeam());
    moveMsg["phase"] = static_cast<int>(m_game->currentPhase());
    sendToAllInRoom(Protocol::makeMsg("game_move", moveMsg));

    if (m_game->isGameOver()) {
        QJsonObject overMsg;
        overMsg["winner"] = static_cast<int>(m_game->getWinner());
        overMsg["winnerName"] = QString::fromUtf8(teamName(m_game->getWinner()));
        sendToAllInRoom(Protocol::makeMsg("game_over", overMsg));
        m_inGame = false;
        m_game.reset();
        return;
    }

    // Process next AI turn (if the next team is AI)
    processAITurns();
}

void GameServer::processAITurns() {
    if (!m_game || !m_inGame) return;
    Team team = m_game->currentTeam();
    int slot = slotForTeam(team);
    if (slot >= 0 && isSlotAI(slot)) {
        m_aiTimer.start(); // delay for visibility
    }
}

void GameServer::onAITimer() {
    if (!m_game || !m_inGame) return;
    Team team = m_game->currentTeam();
    int slot = slotForTeam(team);
    if (slot < 0 || !isSlotAI(slot)) return;

    auto moves = m_game->getLegalMoves();
    if (moves.empty()) {
        m_game->state().advanceTurn();
        processAITurns();
        return;
    }

    Move chosen = AI::chooseBestMove(m_game->state(), moves);
    processGameMove(-1, chosen);

    // Handle chain phases
    int safety = 20;
    while (m_game && m_game->currentPhase() != TurnPhase::ChooseAction && safety-- > 0) {
        auto phaseMoves = m_game->getLegalMoves();
        if (phaseMoves.empty()) break;
        Move pc = AI::chooseBestMove(m_game->state(), phaseMoves);
        processGameMove(-1, pc);
    }
}

int GameServer::slotForTeam(Team team) const {
    int ti = static_cast<int>(team);
    // Direct mapping: slot index == team index
    // Slot 0 = White, Slot 1 = Yellow, Slot 2 = Black, Slot 3 = Red
    if (ti >= 0 && ti < 4 && m_slots[ti].state != Protocol::SlotState::Closed)
        return ti;
    return -1;
}

bool GameServer::isSlotAI(int slot) const {
    return slot >= 0 && slot < 4 && m_slots[slot].state == Protocol::SlotState::AI;
}

QJsonArray GameServer::slotsToJson() const {
    QJsonArray arr;
    for (int i = 0; i < 4; i++) {
        QJsonObject s;
        s["state"] = Protocol::slotStateStr(m_slots[i].state);
        s["name"] = m_slots[i].playerName;
        s["ready"] = m_slots[i].ready;
        s["slot"] = i;
        arr.append(s);
    }
    return arr;
}

QJsonArray GameServer::chatToJson() const {
    QJsonArray arr;
    for (auto& msg : m_chatLog) arr.append(msg);
    return arr;
}

void GameServer::sendTo(int clientId, const QByteArray& data) {
    auto it = m_clients.find(clientId);
    if (it != m_clients.end() && it->socket)
        it->socket->write(data);
}

void GameServer::sendToAll(const QByteArray& data) {
    for (auto& ci : m_clients)
        if (ci.socket) ci.socket->write(data);
}

void GameServer::sendToAllInRoom(const QByteArray& data) {
    sendToAll(data); // single room server
}

void GameServer::broadcastRoomState() {
    QJsonObject state;
    state["slots"] = slotsToJson();
    state["hostId"] = m_hostClientId;
    state["inGame"] = m_inGame;
    state["maxPlayers"] = maxPlayersForMap();
    sendToAll(Protocol::makeMsg("room_state", state));
}

int GameServer::maxPlayersForMap() const {
    const MapDefinition* md = findMap(m_mapId);
    if (md) return md->maxPlayers;
    return 4;
}
