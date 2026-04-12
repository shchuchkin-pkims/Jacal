#include "networkclient.h"
#include <QJsonDocument>

NetworkClient::NetworkClient(QObject* parent) : QObject(parent) {}

NetworkClient::~NetworkClient() { disconnect(); }

void NetworkClient::connectToServer(const QString& host, int port) {
    disconnect();
    m_socket = new QTcpSocket(this);
    QObject::connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    QObject::connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    QObject::connect(m_socket, &QTcpSocket::readyRead, this, &NetworkClient::onDataReady);
    QObject::connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkClient::onSocketError);
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void NetworkClient::disconnect() {
    stopLanDiscovery();
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_isHost = false;
    m_inGame = false;
    m_mySlot = -1;
    m_slots.clear();
    m_chatMessages.clear();
    m_buffer.clear();
    emit connectionChanged();
}

void NetworkClient::setName(const QString& name) {
    send(Protocol::makeMsg("set_name", {{"name", name}}));
}

void NetworkClient::sendChat(const QString& text) {
    send(Protocol::makeMsg("chat", {{"text", text}}));
}

void NetworkClient::setSlot(int slot, const QString& state) {
    QJsonObject d; d["slot"] = slot; d["state"] = state;
    send(Protocol::makeMsg("set_slot", d));
}

void NetworkClient::toggleReady() {
    send(Protocol::makeMsg("ready"));
}

void NetworkClient::requestStartGame(const QString& mapId, float density) {
    QJsonObject data;
    data["mapId"] = mapId;
    data["density"] = density;
    send(Protocol::makeMsg("start_game", data));
}

void NetworkClient::requestSwapSlot(int targetSlot) {
    QJsonObject data;
    data["slot"] = targetSlot;
    send(Protocol::makeMsg("swap_slot", data));
}

void NetworkClient::sendMove(const QJsonObject& moveJson) {
    send(Protocol::makeMsg("game_move", {{"move", moveJson}}));
}

void NetworkClient::startLanDiscovery() {
    stopLanDiscovery();
    m_lanSocket = new QUdpSocket(this);
    m_lanSocket->bind(Protocol::BROADCAST_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    QObject::connect(m_lanSocket, &QUdpSocket::readyRead, this, &NetworkClient::onLanData);
    m_lanServers.clear();
    emit lanServersChanged();
}

void NetworkClient::stopLanDiscovery() {
    if (m_lanSocket) {
        m_lanSocket->close();
        m_lanSocket->deleteLater();
        m_lanSocket = nullptr;
    }
}

void NetworkClient::onConnected() {
    emit connectionChanged();
    emit connected();
}

void NetworkClient::onDisconnected() {
    m_inGame = false;
    emit connectionChanged();
    emit disconnected();
    emit gameStateChanged();
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError) {
    if (m_socket)
        emit errorReceived(m_socket->errorString());
}

void NetworkClient::onDataReady() {
    m_buffer += m_socket->readAll();
    while (true) {
        int nl = m_buffer.indexOf('\n');
        if (nl < 0) break;
        QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer = m_buffer.mid(nl + 1);
        if (line.isEmpty()) continue;
        QJsonObject msg = Protocol::parseMsg(line);
        if (!msg.isEmpty()) handleMessage(msg);
    }
}

void NetworkClient::onLanData() {
    while (m_lanSocket && m_lanSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(m_lanSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        m_lanSocket->readDatagram(data.data(), data.size(), &sender, &senderPort);

        QJsonObject info = QJsonDocument::fromJson(data).object();
        if (info["magic"].toString() != Protocol::BROADCAST_MAGIC) continue;

        QString ip = sender.toString();
        if (ip.startsWith("::ffff:")) ip = ip.mid(7); // strip IPv6 prefix

        // Update or add server
        bool found = false;
        for (int i = 0; i < m_lanServers.size(); i++) {
            QVariantMap srv = m_lanServers[i].toMap();
            if (srv["ip"].toString() == ip && srv["port"].toInt() == info["port"].toInt()) {
                srv["name"] = info["name"].toString();
                srv["players"] = info["players"].toInt();
                srv["inGame"] = info["inGame"].toBool();
                m_lanServers[i] = srv;
                found = true;
                break;
            }
        }
        if (!found) {
            QVariantMap srv;
            srv["ip"] = ip;
            srv["port"] = info["port"].toInt();
            srv["name"] = info["name"].toString();
            srv["players"] = info["players"].toInt();
            srv["maxPlayers"] = info["maxPlayers"].toInt();
            srv["inGame"] = info["inGame"].toBool();
            m_lanServers.append(srv);
        }
        emit lanServersChanged();
    }
}

void NetworkClient::handleMessage(const QJsonObject& msg) {
    QString type = msg["t"].toString();

    if (type == "welcome") {
        m_myClientId = msg["clientId"].toInt();
        m_isHost = msg["isHost"].toBool();
        m_chatMessages.clear();
        QJsonArray chat = msg["chat"].toArray();
        for (auto v : chat) m_chatMessages.append(v.toString());

        QJsonArray sl = msg["slots"].toArray();
        m_slots.clear();
        for (auto v : sl) m_slots.append(v.toObject().toVariantMap());

        // Find my slot
        for (int i = 0; i < m_slots.size(); i++) {
            auto s = m_slots[i].toMap();
            // my slot is the one with my clientId... but server doesn't send clientId in slots
            // Use slot assigned during connection
        }

        emit roomChanged();
        emit chatReceived();
    }
    else if (type == "room_state") {
        QJsonArray sl = msg["slots"].toArray();
        m_slots.clear();
        for (auto v : sl) m_slots.append(v.toObject().toVariantMap());
        m_isHost = (msg["hostId"].toInt() == m_myClientId);

        // Find my slot by name match (simplified)
        for (int i = 0; i < m_slots.size(); i++) {
            auto s = m_slots[i].toMap();
            if (s["state"].toString() == "player") {
                // The server assigns slots in order, my slot was set during welcome
            }
        }
        emit roomChanged();
    }
    else if (type == "chat") {
        QString from = msg["from"].toString();
        QString text = msg["text"].toString();
        m_chatMessages.append(from + ": " + text);
        if (m_chatMessages.size() > 100) m_chatMessages.removeFirst();
        emit chatReceived();
    }
    else if (type == "game_started") {
        int seed = msg["seed"].toInt();
        int numTeams = msg["numTeams"].toInt();
        bool teamMode = msg["teamMode"].toBool();
        QString mapId = msg["mapId"].toString("classic");
        float density = static_cast<float>(msg["density"].toDouble(-1.0));

        // Create local game with same seed and map
        GameConfig cfg;
        cfg.numTeams = numTeams;
        cfg.teamMode = teamMode;
        cfg.seed = static_cast<uint32_t>(seed);
        cfg.mapId = mapId.toStdString();
        cfg.tileDensity = density;
        m_game.newGame(cfg);
        m_inGame = true;
        m_currentTeam = static_cast<int>(m_game.currentTeam());

        emit gameStarted(seed, numTeams, teamMode);
        emit gameStateChanged();
    }
    else if (type == "game_move") {
        QJsonObject moveObj = msg["move"].toObject();
        Move move = Protocol::moveFromJson(moveObj);

        // Apply to local game
        auto legal = m_game.getLegalMoves();
        for (auto& lm : legal) {
            if (lm.type == move.type && lm.to == move.to && lm.pirateId == move.pirateId) {
                m_game.makeMove(lm);
                break;
            }
        }
        m_currentTeam = static_cast<int>(m_game.currentTeam());

        emit gameMoveReceived(msg);
        emit gameStateChanged();
    }
    else if (type == "game_over") {
        m_inGame = false;
        emit gameOver(msg["winnerName"].toString());
        emit gameStateChanged();
    }
    else if (type == "error") {
        emit errorReceived(msg["msg"].toString());
    }
}

void NetworkClient::send(const QByteArray& data) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->write(data);
}
