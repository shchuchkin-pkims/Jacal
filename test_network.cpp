#include <QCoreApplication>
#include <QThread>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include "gameserver.h"
#include "protocol.h"

static int passed = 0, failed = 0;
static void CHECK(bool c, const char* m) {
    if (c) { std::cout << "  PASS: " << m << "\n"; passed++; }
    else   { std::cout << "  FAIL: " << m << "\n"; failed++; }
}

static QList<QJsonObject> readMsgs(QTcpSocket& s, int ms = 800) {
    QList<QJsonObject> result;
    for (int i = 0; i < 20; i++) {
        QCoreApplication::processEvents();
        QThread::msleep(ms / 20);
    }
    QCoreApplication::processEvents();
    QByteArray buf = s.readAll();
    for (auto& line : buf.split('\n')) {
        if (line.isEmpty()) continue;
        auto obj = QJsonDocument::fromJson(line).object();
        if (!obj.isEmpty()) result.append(obj);
    }
    return result;
}

static void sendMsg(QTcpSocket& s, const QString& type, const QJsonObject& extra = {}) {
    s.write(Protocol::makeMsg(type, extra));
    s.flush();
}

static bool hasMsgType(const QList<QJsonObject>& msgs, const QString& t) {
    for (auto& m : msgs) if (m["t"].toString() == t) return true;
    return false;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // === TEST 1: Server starts ===
    std::cout << "\n=== TEST 1: Server start ===\n";
    GameServer server;
    server.setRoomName("TestRoom");
    CHECK(server.start(12345), "Server started");
    CHECK(server.isRunning(), "Server is running");
    QCoreApplication::processEvents();

    // === TEST 2: Host connects ===
    std::cout << "\n=== TEST 2: Host connects ===\n";
    QTcpSocket host;
    host.connectToHost("127.0.0.1", 12345);
    CHECK(host.waitForConnected(3000), "Host connected");

    QJsonObject nameObj; nameObj["name"] = "HostPlayer";
    sendMsg(host, "set_name", nameObj);

    auto msgs = readMsgs(host, 500);
    std::cout << "  Host got " << msgs.size() << " messages: ";
    for (auto& m : msgs) std::cout << m["t"].toString().toStdString() << " ";
    std::cout << "\n";
    CHECK(!msgs.isEmpty(), "Host received initial messages");
    CHECK(hasMsgType(msgs, "room_state") || hasMsgType(msgs, "welcome"), "Got room/welcome");

    // === TEST 3: Configure lobby (1v1 vs AI) ===
    std::cout << "\n=== TEST 3: Lobby config ===\n";
    QJsonObject s1; s1["slot"] = 1; s1["state"] = "ai";
    sendMsg(host, "set_slot", s1);
    QJsonObject s2; s2["slot"] = 2; s2["state"] = "closed";
    sendMsg(host, "set_slot", s2);
    QJsonObject s3; s3["slot"] = 3; s3["state"] = "closed";
    sendMsg(host, "set_slot", s3);

    msgs = readMsgs(host, 500);
    bool gotRoom = hasMsgType(msgs, "room_state");
    CHECK(gotRoom, "Room update after slot config");
    if (gotRoom) {
        for (auto& m : msgs) {
            if (m["t"].toString() != "room_state") continue;
            QJsonArray slotArr = m["slots"].toArray();
            std::cout << "  Slots (" << slotArr.size() << "): ";
            for (int i = 0; i < slotArr.size(); i++) {
                QJsonObject so = slotArr[i].toObject();
                std::cout << so["state"].toString().toStdString() << " ";
            }
            std::cout << "\n";
        }
    }

    // === TEST 4: Ready + Start ===
    std::cout << "\n=== TEST 4: Start game ===\n";
    sendMsg(host, "ready");
    QThread::msleep(200); QCoreApplication::processEvents();

    sendMsg(host, "start_game");
    msgs = readMsgs(host, 1000);

    std::cout << "  After start, got: ";
    for (auto& m : msgs) std::cout << m["t"].toString().toStdString() << " ";
    std::cout << "\n";

    CHECK(hasMsgType(msgs, "game_started"), "game_started received");

    bool hasTurn = hasMsgType(msgs, "your_turn") || hasMsgType(msgs, "wait_turn")
                || hasMsgType(msgs, "game_state");
    CHECK(hasTurn, "Turn notification received");

    // === TEST 5: Two players ===
    std::cout << "\n=== TEST 5: Two-player lobby ===\n";
    server.stop();
    QThread::msleep(200); QCoreApplication::processEvents();

    GameServer server2;
    server2.setRoomName("2P");
    CHECK(server2.start(12346), "Server2 started on 12346");

    QTcpSocket p1, p2;
    p1.connectToHost("127.0.0.1", 12346);
    CHECK(p1.waitForConnected(3000), "P1 connected");
    QJsonObject n1; n1["name"] = "Alice";
    sendMsg(p1, "set_name", n1);
    readMsgs(p1, 300);

    p2.connectToHost("127.0.0.1", 12346);
    CHECK(p2.waitForConnected(3000), "P2 connected");
    QJsonObject n2; n2["name"] = "Bob";
    sendMsg(p2, "set_name", n2);
    readMsgs(p2, 300);

    // Close extra slots
    QJsonObject cs2; cs2["slot"] = 2; cs2["state"] = "closed";
    QJsonObject cs3; cs3["slot"] = 3; cs3["state"] = "closed";
    sendMsg(p1, "set_slot", cs2);
    sendMsg(p1, "set_slot", cs3);
    QThread::msleep(300); QCoreApplication::processEvents();

    sendMsg(p1, "ready");
    sendMsg(p2, "ready");
    QThread::msleep(300); QCoreApplication::processEvents();

    sendMsg(p1, "start_game");
    auto m1 = readMsgs(p1, 1000);
    auto m2 = readMsgs(p2, 500);

    CHECK(hasMsgType(m1, "game_started"), "P1 got game_started");
    CHECK(hasMsgType(m2, "game_started"), "P2 got game_started");

    std::cout << "  P1 msgs: ";
    for (auto& m : m1) std::cout << m["t"].toString().toStdString() << " ";
    std::cout << "\n  P2 msgs: ";
    for (auto& m : m2) std::cout << m["t"].toString().toStdString() << " ";
    std::cout << "\n";

    server2.stop();

    // === RESULTS ===
    std::cout << "\n============================\n";
    std::cout << "PASSED: " << passed << "  FAILED: " << failed << "\n";
    std::cout << "============================\n";
    return failed > 0 ? 1 : 0;
}

#include "test_network.moc"
