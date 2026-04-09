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

        // "main" = top menu, "single" = single player submenu, "sandbox" = sandbox
        property string menuState: "main"

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

            // ========== TOP MENU ==========
            Column {
                width: parent.width
                spacing: 8
                visible: startScreen.menuState === "main"

                Button {
                    width: parent.width; height: 42; text: "Одиночная игра"; font.pixelSize: 16
                    onClicked: startScreen.menuState = "single"
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "white"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    width: parent.width; height: 42; text: "Сетевая игра"; font.pixelSize: 16
                    onClicked: gameController.showNetworkScreen()
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "#aaddff"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    width: parent.width; height: 42; text: "Песочница"; font.pixelSize: 16
                    onClicked: startScreen.menuState = "sandbox"
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "white"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    width: parent.width; height: 42; text: "Редактор карт"; font.pixelSize: 16
                    onClicked: gameController.showMapEditor()
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "white"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Item { width: 1; height: 4 }

                Button {
                    width: parent.width; height: 38; text: "Проверить обновления"; font.pixelSize: 14
                    onClicked: updateChecker.checkForUpdates()
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "#999"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Button {
                    width: parent.width; height: 38; text: "Выход"; font.pixelSize: 14
                    onClicked: Qt.quit()
                    background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                    contentItem: Text { text: parent.text; color: "#999"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                Text {
                    text: "v" + updateChecker.currentVersion
                    color: "#555"; font.pixelSize: 11
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // ========== SINGLE PLAYER SUBMENU ==========
            Column {
                id: singlePlayerMenu
                width: parent.width
                spacing: 8
                visible: startScreen.menuState === "single"

                // Map selector
                RowLayout {
                    width: parent.width; spacing: 8
                    Text { text: "Карта:"; color: "#ccc"; font.pixelSize: 14
                        Layout.alignment: Qt.AlignVCenter }
                    ComboBox {
                        id: mapSelector
                        Layout.fillWidth: true; height: 36
                        model: {
                            var maps = gameController.availableMaps();
                            var names = [];
                            for (var i = 0; i < maps.length; i++) names.push(maps[i].name);
                            return names;
                        }
                        property var mapIds: {
                            var maps = gameController.availableMaps();
                            var ids = [];
                            for (var i = 0; i < maps.length; i++) ids.push(maps[i].id);
                            return ids;
                        }
                        property string selectedMapId: mapIds.length > 0 ? mapIds[currentIndex] : "classic"
                        currentIndex: 0

                        background: Rectangle { color: "#2a4a6a"; radius: 5; border.color: "#5a8aba" }
                        contentItem: Text {
                            text: mapSelector.displayText; color: "white"; font.pixelSize: 14
                            leftPadding: 8; verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                // Map preview
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width * 0.6, 150); height: width
                    source: gameController.mapPreviewUrl(mapSelector.selectedMapId)
                    visible: status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                }

                // Tile density slider (Improvement 4)
                RowLayout {
                    width: parent.width; spacing: 6
                    Text { text: "Плотность:"; color: "#aaa"; font.pixelSize: 12
                        Layout.alignment: Qt.AlignVCenter }
                    Slider {
                        id: densitySlider
                        Layout.fillWidth: true; height: 28
                        from: 0.0; to: 1.0; value: 0.85; stepSize: 0.05
                        // 0.85 ≈ default ratio (100 filled / 117 tiles)
                    }
                    Text { text: Math.round(densitySlider.value * 100) + "%"
                        color: "#ffd700"; font.pixelSize: 12; font.bold: true
                        Layout.alignment: Qt.AlignVCenter; Layout.preferredWidth: 35 }
                }
                Text {
                    text: densitySlider.value < 0.2 ? "Почти пустой остров" :
                          densitySlider.value > 0.95 ? "Максимум событий!" :
                          densitySlider.value < 0.5 ? "Спокойная игра" : "Стандарт"
                    color: "#888"; font.pixelSize: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Game mode buttons (filtered by map maxPlayers, Bug 6)
                property int mapMaxPlayers: {
                    var maps = gameController.availableMaps();
                    for (var i = 0; i < maps.length; i++)
                        if (maps[i].id === mapSelector.selectedMapId) return maps[i].maxPlayers;
                    return 4;
                }

                Repeater {
                    model: [
                        {text: "1 vs ИИ (дуэль)",          players: 2, team: false, ai: true},
                        {text: "2 игрока (дуэль)",          players: 2, team: false, ai: false},
                        {text: "1 vs ИИ (команды 2x2)",     players: 4, team: true,  ai: true},
                        {text: "2 игрока (команды 2x2)",     players: 4, team: true,  ai: false},
                        {text: "3 игрока",                   players: 3, team: false, ai: false},
                        {text: "4 игрока",                   players: 4, team: false, ai: false}
                    ]
                    Button {
                        width: parent.width; height: 36
                        text: modelData.text; font.pixelSize: 13
                        visible: modelData.players <= singlePlayerMenu.mapMaxPlayers
                        enabled: modelData.players <= singlePlayerMenu.mapMaxPlayers
                        onClicked: gameController.newGameWithDensity(
                            modelData.players, modelData.team, modelData.ai,
                            mapSelector.selectedMapId, densitySlider.value)
                        background: Rectangle {
                            color: parent.enabled ? (parent.hovered ? "#3a6a9a" : "#2a4a6a") : "#1a2a3a"
                            radius: 5; border.color: parent.enabled ? "#5a8aba" : "#3a4a5a"
                        }
                        contentItem: Text { text: parent.text
                            color: parent.enabled ? "white" : "#555"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }

                Item { width: 1; height: 4 }

                Button {
                    width: parent.width; height: 36; text: "\u25C0  Назад"; font.pixelSize: 14
                    onClicked: startScreen.menuState = "main"
                    background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 5 }
                    contentItem: Text { text: parent.text; color: "#aaa"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }

            // ========== SANDBOX ==========
            Column {
                width: parent.width
                spacing: 4
                visible: startScreen.menuState === "sandbox"

                Text {
                    text: "Выберите тип клетки для тестирования:"
                    color: "#aaa"; font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Rectangle {
                    width: parent.width; height: 360
                    color: "#1a3050"; radius: 6; clip: true

                    Flickable {
                        anchors.fill: parent; anchors.margins: 6
                        contentHeight: sandboxCol.height; clip: true

                        Column {
                            id: sandboxCol
                            width: parent.width; spacing: 3

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
                    text: "Тестовые клетки рядом с кораблями и в центре.\nСундуки на (1,2) и (1,10)."
                    color: "#888"; font.pixelSize: 11; wrapMode: Text.WordWrap
                    width: parent.width; horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    width: parent.width; height: 36; text: "\u25C0  Назад"; font.pixelSize: 14
                    onClicked: startScreen.menuState = "main"
                    background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 5 }
                    contentItem: Text { text: parent.text; color: "#aaa"; font: parent.font
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }

    // ===== NETWORK SCREEN =====
    NetworkScreen {
        id: networkScreen
        anchors.fill: parent
        visible: false
    }

    // ===== MAP EDITOR =====
    MapEditor {
        id: mapEditor
        anchors.fill: parent
        visible: false
    }

    // ===== UPDATE DIALOG =====
    Rectangle {
        id: updateDialog
        anchors.fill: parent; color: "#cc000000"; visible: false; z: 100

        Rectangle {
            anchors.centerIn: parent; width: 400; height: updateCol.height + 40
            color: "#1a3050"; radius: 10; border.color: "#4a7aba"; border.width: 2

            Column {
                id: updateCol
                anchors.centerIn: parent; spacing: 12; width: 360

                Text {
                    text: updateChecker.updateAvailable ? "Доступно обновление!" : "Проверка обновлений"
                    font.pixelSize: 20; font.bold: true; color: "#ffd700"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: updateChecker.checking ? "Проверяю..." :
                          updateChecker.updateAvailable ?
                            "Текущая: v" + updateChecker.currentVersion + "\nНовая: v" + updateChecker.latestVersion :
                            "У вас актуальная версия (v" + updateChecker.currentVersion + ")"
                    color: "#ccc"; font.pixelSize: 14; wrapMode: Text.WordWrap; width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                }
                ProgressBar { width: parent.width; visible: updateChecker.downloading; value: updateChecker.downloadProgress / 100.0 }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter; spacing: 12
                    Button {
                        text: "Обновить"; width: 140; visible: updateChecker.updateAvailable && !updateChecker.downloading
                        onClicked: updateChecker.downloadUpdate()
                        background: Rectangle { color: parent.hovered ? "#4a8a4a" : "#2a6a2a"; radius: 5 }
                        contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        text: "Закрыть"; width: 140; visible: !updateChecker.downloading
                        onClicked: updateDialog.visible = false
                        background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 5 }
                        contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }
    }

    Connections {
        target: updateChecker
        function onUpdateChecked() {
            if (updateChecker.updateAvailable || updateDialog.visible) updateDialog.visible = true
        }
    }

    // Auto-check updates on startup (silent)
    Component.onCompleted: {
        updateChecker.checkForUpdates()
        // No auto LAN discovery — user clicks "Обновить" in network screen
    }

    // Screen switching
    Connections {
        target: gameController
        function onScreenChanged(screen) {
            networkScreen.visible = (screen === "network")
            mapEditor.visible = (screen === "editor")
            startScreen.visible = (screen === "main")
            if (screen === "main") {
                startScreen.menuState = "main"
                networkClient.stopLanDiscovery()
            }
        }
    }

    // Network game started
    Connections {
        target: networkClient
        function onGameStarted(seed, numTeams, teamMode) { networkScreen.visible = false }
        function onErrorReceived(msg) { msgText.text = msg; msgPopup.visible = true; msgTimer.restart() }
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
            if (!gameController.gameActive) startScreen.visible = true
            else { startScreen.visible = false; gameOverOverlay.visible = false }
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
