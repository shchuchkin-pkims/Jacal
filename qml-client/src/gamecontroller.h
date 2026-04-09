#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QTimer>
#include <array>
#include "game.h"
#include "ai.h"
#include "gameserver.h"

class BoardModel;

class GameController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gameActive READ gameActive NOTIFY gameChanged)
    Q_PROPERTY(QString currentTeamName READ currentTeamName NOTIFY turnChanged)
    Q_PROPERTY(QString currentTeamColor READ currentTeamColor NOTIFY turnChanged)
    Q_PROPERTY(int turnNumber READ turnNumber NOTIFY turnChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString phaseText READ phaseText NOTIFY statusChanged)
    Q_PROPERTY(QVariantList scores READ scores NOTIFY scoresChanged)
    Q_PROPERTY(QVariantList pirates READ pirates NOTIFY boardChanged)
    Q_PROPERTY(QVariantList shipList READ shipList NOTIFY boardChanged)
    Q_PROPERTY(QVariantList validTargets READ validTargets NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool isAITurn READ isAITurn NOTIFY turnChanged)
    Q_PROPERTY(QString assetsPath READ assetsPath CONSTANT)
    Q_PROPERTY(QString portraitsPath READ portraitsPath CONSTANT)
    Q_PROPERTY(QStringList moveLog READ moveLog NOTIFY logChanged)
    Q_PROPERTY(QVariantList crewStatus READ crewStatus NOTIFY boardChanged)
    Q_PROPERTY(int currentTeamRum READ currentTeamRum NOTIFY boardChanged)
    Q_PROPERTY(bool rumUseMode READ rumUseMode NOTIFY selectionChanged)

public:
    explicit GameController(QObject* parent = nullptr);

    void setBoardModel(BoardModel* model) { m_boardModel = model; }
    void setNetworkClient(class NetworkClient* nc);

    Q_INVOKABLE void newGame(int numPlayers, bool teamMode, bool vsAI, const QString& mapId = "classic");
    Q_INVOKABLE void newGameWithDensity(int numPlayers, bool teamMode, bool vsAI, const QString& mapId, float density);
    Q_INVOKABLE void newSandbox(int tileTypeId, int dirBits, int value);
    Q_INVOKABLE QVariantList sandboxTileTypes() const;
    Q_INVOKABLE QVariantList availableMaps() const;
    Q_INVOKABLE void cellClicked(int row, int col);
    Q_INVOKABLE void moveShipLeft();
    Q_INVOKABLE void moveShipRight();
    Q_INVOKABLE void cancelSelection();
    Q_INVOKABLE void activateRumUse();
    Q_INVOKABLE void cancelRumUse();
    Q_INVOKABLE void resurrectPirate(int pirateIndex);
    Q_INVOKABLE void hostGame(const QString& roomName, int port = 12345);
    Q_INVOKABLE void saveCustomMap(const QString& name, const QString& terrain, const QString& ships);
    Q_INVOKABLE void showMainMenu();
    Q_INVOKABLE void showMapEditor();
    Q_INVOKABLE void showNetworkScreen();

    bool gameActive() const { return m_gameActive; }
    QString currentTeamName() const;
    QString currentTeamColor() const;
    int turnNumber() const;
    QString statusText() const;
    QString phaseText() const;
    QVariantList scores() const;
    QVariantList pirates() const;
    QVariantList shipList() const;
    QVariantList validTargets() const;
    bool hasSelection() const { return m_selectedPirate.valid() || m_rumUseMode; }
    bool isAITurn() const;
    bool rumUseMode() const { return m_rumUseMode; }
    QString assetsPath() const;
    Q_INVOKABLE QString fileUrl(const QString& relativePath) const;
    Q_INVOKABLE QString mapPreviewUrl(const QString& mapId) const;
    QString portraitsPath() const;
    QStringList moveLog() const { return m_moveLog; }
    QVariantList crewStatus() const;
    int currentTeamRum() const;
    Q_INVOKABLE void quitToMenu();
    Q_INVOKABLE void selectCrewMember(int team, int index);

signals:
    void gameChanged();
    void turnChanged();
    void statusChanged();
    void scoresChanged();
    void boardChanged();
    void selectionChanged();
    void logChanged();
    void gameOver(QString winner);
    void showMessage(QString msg);
    void screenChanged(QString screen); // "main", "network"

private slots:
    void processAITurn();
    void onNetworkGameStarted(int seed, int numTeams, bool teamMode);
    void onNetworkMoveReceived(QJsonObject moveData);
    void onNetworkGameOver(QString winnerName);

private:
    void updateAll();
    void processEvents(const EventList& events);
    void selectPirate(const PirateId& id);
    void clearSelection();
    void tryExecuteMove(Coord target);
    void doShipMove(int direction);
    void scheduleAIIfNeeded();
    void addLog(const QString& msg);
    QString describeMoveResult(const Move& move, const EventList& events);
    void generateMapPreview(const MapDefinition& md, const QString& pngPath);

    Game m_game;
    BoardModel* m_boardModel = nullptr;
    bool m_gameActive = false;

    PirateId m_selectedPirate;
    MoveList m_legalMoves;
    std::vector<Move> m_movesForSelected;
    std::vector<Coord> m_validTargetCoords;
    QStringList m_moveLog;
    bool m_rumUseMode = false;
    std::vector<Move> m_rumMoves; // UseRum moves available

    // AI
    std::array<bool, MAX_TEAMS> m_isAI = {};
    QTimer m_aiTimer;
    float m_pendingDensity = -1.0f;

    // Network
    GameServer* m_server = nullptr;
    class NetworkClient* m_networkClient = nullptr;
    bool m_isNetworkGame = false;
    int m_myNetworkTeam = -1;
    std::array<bool, MAX_TEAMS> m_netSlotIsAI = {};  // which teams are AI in network game
};
