#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include "protocol.h"
#include "game.h"

class NetworkClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY roomChanged)
    Q_PROPERTY(bool inGame READ inGame NOTIFY gameStateChanged)
    Q_PROPERTY(QVariantList roomSlots READ roomSlots NOTIFY roomChanged)
    Q_PROPERTY(QStringList chatMessages READ chatMessages NOTIFY chatReceived)
    Q_PROPERTY(QVariantList lanServers READ lanServers NOTIFY lanServersChanged)
    Q_PROPERTY(int mySlot READ mySlot NOTIFY roomChanged)
    Q_PROPERTY(int currentTeam READ currentNetTeam NOTIFY gameStateChanged)
    Q_PROPERTY(QString lobbyMapId READ lobbyMapId NOTIFY roomChanged)

public:
    explicit NetworkClient(QObject* parent = nullptr);
    ~NetworkClient();

    Q_INVOKABLE void connectToServer(const QString& host, int port = Protocol::DEFAULT_PORT);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void setName(const QString& name);
    Q_INVOKABLE void sendChat(const QString& text);
    Q_INVOKABLE void setSlot(int slot, const QString& state);
    Q_INVOKABLE void toggleReady();
    Q_INVOKABLE void requestStartGame(const QString& mapId = "classic", float density = -1.0f);
    Q_INVOKABLE void requestSwapSlot(int targetSlot);
    Q_INVOKABLE void sendMove(const QJsonObject& moveJson);
    Q_INVOKABLE void sendMapChange(const QString& mapId);
    Q_INVOKABLE void startLanDiscovery();
    Q_INVOKABLE void stopLanDiscovery();

    bool isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }
    bool isHost() const { return m_isHost; }
    bool inGame() const { return m_inGame; }
    QVariantList roomSlots() const { return m_slots; }
    QStringList chatMessages() const { return m_chatMessages; }
    QVariantList lanServers() const { return m_lanServers; }
    int mySlot() const { return m_mySlot; }
    int currentNetTeam() const { return m_currentTeam; }
    QString lobbyMapId() const { return m_lobbyMapId; }

    // Access local game for network play
    Game& game() { return m_game; }

signals:
    void connectionChanged();
    void roomChanged();
    void chatReceived();
    void lanServersChanged();
    void gameStateChanged();
    void gameStarted(int seed, int numTeams, bool teamMode);
    void gameMoveReceived(QJsonObject moveData);
    void gameOver(QString winnerName);
    void errorReceived(QString msg);
    void connected();
    void disconnected();

private slots:
    void onConnected();
    void onDisconnected();
    void onDataReady();
    void onLanData();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    void handleMessage(const QJsonObject& msg);
    void send(const QByteArray& data);

    QTcpSocket* m_socket = nullptr;
    QUdpSocket* m_lanSocket = nullptr;
    QByteArray m_buffer;

    bool m_isHost = false;
    bool m_inGame = false;
    int m_myClientId = -1;
    int m_mySlot = -1;
    int m_currentTeam = 0;
    QVariantList m_slots;
    QStringList m_chatMessages;
    QVariantList m_lanServers;
    QString m_lobbyMapId = "classic";
    Game m_game; // local game instance for network play
};
