#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QMap>
#include <memory>
#include "game.h"
#include "ai.h"
#include "protocol.h"

struct ClientInfo {
    QTcpSocket* socket = nullptr;
    QString name;
    int slot = -1; // assigned slot (0-3), -1 = spectator
};

struct RoomSlot {
    Protocol::SlotState state = Protocol::SlotState::Open;
    QString playerName;
    int clientId = -1; // -1 if AI or closed
    bool ready = false;
};

class GameServer : public QObject {
    Q_OBJECT
public:
    explicit GameServer(QObject* parent = nullptr);
    ~GameServer();

    bool start(quint16 port = Protocol::DEFAULT_PORT);
    void stop();
    bool isRunning() const { return m_server && m_server->isListening(); }
    quint16 port() const { return m_port; }

    void setRoomName(const QString& name) { m_roomName = name; }
    QString roomName() const { return m_roomName; }

signals:
    void logMessage(const QString& msg);

private slots:
    void onNewConnection();
    void onClientData();
    void onClientDisconnected();
    void onBroadcastTimer();
    void onAITimer();

private:
    void handleMessage(int clientId, const QJsonObject& msg);
    void sendTo(int clientId, const QByteArray& data);
    void sendToAll(const QByteArray& data);
    void sendToAllInRoom(const QByteArray& data);
    void broadcastRoomState();
    void processGameMove(int clientId, const Move& move);
    void processAITurns();
    int slotForTeam(Team team) const;
    bool isSlotAI(int slot) const;
    QJsonArray slotsToJson() const;
    QJsonArray chatToJson() const;

    QTcpServer* m_server = nullptr;
    QUdpSocket* m_broadcastSocket = nullptr;
    QTimer m_broadcastTimer;
    QTimer m_aiTimer;
    quint16 m_port = Protocol::DEFAULT_PORT;
    QString m_roomName = "Jacal Game";

    int m_nextClientId = 1;
    QMap<int, ClientInfo> m_clients;

    // Room state
    RoomSlot m_slots[4];
    bool m_inGame = false;
    int m_hostClientId = -1;
    QStringList m_chatLog;

    // Game (active during gameplay)
    std::unique_ptr<Game> m_game;
    uint32_t m_gameSeed = 0;
};
