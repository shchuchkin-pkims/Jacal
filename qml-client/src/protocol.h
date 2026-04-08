#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "types.h"

namespace Protocol {

// Current version
inline const char* APP_VERSION = "0.2.0";
inline const char* GITHUB_REPO = "JacalGame/Jacal"; // owner/repo
inline quint16 DEFAULT_PORT = 12345;
inline quint16 BROADCAST_PORT = 12346;
inline const char* BROADCAST_MAGIC = "JACAL_SERVER_V1";

// Slot states in lobby
enum class SlotState { Open, Closed, AI, Player };

inline QString slotStateStr(SlotState s) {
    switch (s) {
        case SlotState::Open: return "open";
        case SlotState::Closed: return "closed";
        case SlotState::AI: return "ai";
        case SlotState::Player: return "player";
    }
    return "open";
}
inline SlotState slotStateFromStr(const QString& s) {
    if (s == "closed") return SlotState::Closed;
    if (s == "ai") return SlotState::AI;
    if (s == "player") return SlotState::Player;
    return SlotState::Open;
}

// Move serialization
inline QJsonObject moveToJson(const Move& m) {
    QJsonObject o;
    o["type"] = static_cast<int>(m.type);
    o["pt"] = static_cast<int>(m.pirateId.team);
    o["pi"] = m.pirateId.index;
    o["fr"] = m.from.row; o["fc"] = m.from.col;
    o["tr"] = m.to.row; o["tc"] = m.to.col;
    o["dir"] = static_cast<int>(m.chosenDir);
    o["ci"] = m.characterIndex;
    return o;
}

inline Move moveFromJson(const QJsonObject& o) {
    Move m;
    m.type = static_cast<MoveType>(o["type"].toInt());
    m.pirateId.team = static_cast<Team>(o["pt"].toInt());
    m.pirateId.index = o["pi"].toInt();
    m.from = {o["fr"].toInt(), o["fc"].toInt()};
    m.to = {o["tr"].toInt(), o["tc"].toInt()};
    m.chosenDir = static_cast<Direction>(o["dir"].toInt(-1));
    m.characterIndex = o["ci"].toInt(-1);
    return m;
}

// Message helpers
inline QByteArray makeMsg(const QString& type, const QJsonObject& data = {}) {
    QJsonObject msg;
    msg["t"] = type;
    for (auto it = data.begin(); it != data.end(); ++it)
        msg[it.key()] = it.value();
    return QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
}

inline QJsonObject parseMsg(const QByteArray& data) {
    return QJsonDocument::fromJson(data).object();
}

} // namespace Protocol
