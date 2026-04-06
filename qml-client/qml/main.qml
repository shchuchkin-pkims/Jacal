import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 1100
    height: 800
    visible: true
    title: "Jacal — Treasure Island"
    color: "#0a1e3d"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        GameBoard {
            id: gameBoard
            Layout.fillHeight: true
            Layout.preferredWidth: height
            Layout.minimumWidth: 400
        }

        HUD {
            id: hud
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 220
            Layout.maximumWidth: 350
        }
    }

    // ===== START SCREEN =====
    Rectangle {
        id: startScreen
        anchors.fill: parent
        color: "#dd0a1e3d"
        visible: !gameController.gameActive

        property bool showSandbox: false

        Column {
            anchors.centerIn: parent
            spacing: 12
            width: 400

            Text {
                text: "JACAL"
                font.pixelSize: 52; font.bold: true; color: "#ffd700"
                anchors.horizontalCenter: parent.horizontalCenter
                style: Text.Outline; styleColor: "#000"
            }
            Text {
                text: "Treasure Island"
                font.pixelSize: 20; color: "#ddd"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item { width: 1; height: 10 }

            // Tab buttons
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                Button {
                    text: "Игра"
                    width: 120; height: 36
                    font.pixelSize: 14
                    onClicked: startScreen.showSandbox = false
                    background: Rectangle {
                        color: !startScreen.showSandbox ? "#4a7aba" : "#2a4a6a"
                        radius: 5
                    }
                    contentItem: Text { text: parent.text; color: "white"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    text: "Песочница"
                    width: 120; height: 36
                    font.pixelSize: 14
                    onClicked: startScreen.showSandbox = true
                    background: Rectangle {
                        color: startScreen.showSandbox ? "#4a7aba" : "#2a4a6a"
                        radius: 5
                    }
                    contentItem: Text { text: parent.text; color: "white"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }

            Item { width: 1; height: 5 }

            // === GAME MODES ===
            Column {
                width: parent.width
                spacing: 8
                visible: !startScreen.showSandbox

                Repeater {
                    model: [
                        {text: "1 vs ИИ (дуэль, 2 корабля)",  players: 2, team: false, ai: true},
                        {text: "2 игрока (дуэль, 2 корабля)",  players: 2, team: false, ai: false},
                        {text: "1 vs ИИ (команды 2x2)",        players: 4, team: true,  ai: true},
                        {text: "2 игрока (команды 2x2)",        players: 4, team: true,  ai: false},
                        {text: "3 игрока",                      players: 3, team: false, ai: false},
                        {text: "4 игрока",                      players: 4, team: false, ai: false}
                    ]
                    Button {
                        width: parent.width; height: 38
                        text: modelData.text; font.pixelSize: 14
                        onClicked: gameController.newGame(modelData.players, modelData.team, modelData.ai)
                        background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                        contentItem: Text { text: parent.text; color: "white"; font: parent.font
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }

            // === SANDBOX ===
            Column {
                width: parent.width
                spacing: 4
                visible: startScreen.showSandbox

                Text {
                    text: "Выберите тип клетки для тестирования:"
                    color: "#aaa"; font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Rectangle {
                    width: parent.width
                    height: 360
                    color: "#1a3050"
                    radius: 6
                    clip: true

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 6
                        contentHeight: sandboxCol.height
                        clip: true

                        Column {
                            id: sandboxCol
                            width: parent.width
                            spacing: 3

                            Repeater {
                                model: gameController.sandboxTileTypes()

                                Button {
                                    width: parent.width; height: 32
                                    text: modelData.name; font.pixelSize: 13
                                    onClicked: gameController.newSandbox(modelData.id, modelData.dir, modelData.val)
                                    background: Rectangle {
                                        color: parent.hovered ? "#3a6050" : "#253545"
                                        radius: 4; border.color: "#4a8a6a"
                                    }
                                    contentItem: Text { text: parent.text; color: "#aaffaa"; font: parent.font
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "Поле заполнено открытыми пустыми клетками.\nТестовые клетки закрыты — рядом с кораблями и в центре.\nСундуки на (1,2) и (1,10) для теста монет."
                    color: "#888"; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Item { width: 1; height: 10 }

            Button {
                width: parent.width; height: 38
                text: "Выход"
                font.pixelSize: 14
                onClicked: Qt.quit()
                background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                contentItem: Text { text: parent.text; color: "white"; font: parent.font
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
    }

    // ===== GAME OVER =====
    Rectangle {
        id: gameOverOverlay
        anchors.fill: parent; color: "#cc000000"; visible: false

        Column {
            anchors.centerIn: parent; spacing: 20
            Text { text: gameOverText; font.pixelSize: 36; font.bold: true; color: "#ffd700"
                anchors.horizontalCenter: parent.horizontalCenter }
            Button {
                text: "Меню"; width: 200; anchors.horizontalCenter: parent.horizontalCenter
                onClicked: { gameOverOverlay.visible = false; gameController.newGame(0, false, false) }
                background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 6 }
                contentItem: Text { text: parent.text; color: "white"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
    }

    property string gameOverText: ""
    Connections {
        target: gameController
        function onGameOver(winner) { gameOverText = "Победа: " + winner + "!"; gameOverOverlay.visible = true }
        function onGameChanged() {
            if (!gameController.gameActive) {
                startScreen.visible = true
            } else {
                // New game started — hide all overlays
                startScreen.visible = false
                gameOverOverlay.visible = false
            }
        }
    }

    // ===== MESSAGE POPUP =====
    Rectangle {
        id: msgPopup
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 20
        width: msgText.width + 30; height: msgText.height + 16
        color: "#cc2a4a6a"; radius: 8; visible: false

        Text { id: msgText; anchors.centerIn: parent; color: "white"; font.pixelSize: 14 }
        Timer { id: msgTimer; interval: 3000; onTriggered: msgPopup.visible = false }
    }
    Connections {
        target: gameController
        function onShowMessage(msg) { msgText.text = msg; msgPopup.visible = true; msgTimer.restart() }
    }
}
