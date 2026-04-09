import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: netScreen
    color: "#0e2240"

    property bool inLobby: networkClient.connected
    property bool inGame: networkClient.inGame

    // ===== SERVER BROWSER (not in lobby) =====
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        visible: !inLobby

        Text {
            text: "Сетевая игра"
            font.pixelSize: 28; font.bold: true; color: "#ffd700"
            Layout.alignment: Qt.AlignHCenter
        }

        // Direct connect
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Text { text: "IP:"; color: "#ccc"; font.pixelSize: 14 }
            TextField {
                id: ipField; Layout.fillWidth: true
                text: "localhost"; color: "white"; font.pixelSize: 14
                background: Rectangle { color: "#1a3a5a"; radius: 4; border.color: "#4a7aba" }
            }
            Text { text: "Порт:"; color: "#ccc"; font.pixelSize: 14 }
            TextField {
                id: portField; width: 70
                text: "12345"; color: "white"; font.pixelSize: 14
                validator: IntValidator { bottom: 1; top: 65535 }
                background: Rectangle { color: "#1a3a5a"; radius: 4; border.color: "#4a7aba" }
            }
            Button {
                text: "Подключиться"; width: 130
                onClicked: networkClient.connectToServer(ipField.text, parseInt(portField.text))
                background: Rectangle { color: parent.hovered ? "#3a6a9a" : "#2a4a6a"; radius: 5 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // Host game
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Text { text: "Название:"; color: "#ccc"; font.pixelSize: 14 }
            TextField {
                id: roomNameField; Layout.fillWidth: true
                text: "Jacal Game"; color: "white"; font.pixelSize: 14
                background: Rectangle { color: "#1a3a5a"; radius: 4; border.color: "#4a7aba" }
            }
            Text { text: "Порт:"; color: "#ccc"; font.pixelSize: 14 }
            TextField {
                id: hostPortField; width: 70
                text: "12345"; color: "white"; font.pixelSize: 14
                validator: IntValidator { bottom: 1; top: 65535 }
                background: Rectangle { color: "#1a3a5a"; radius: 4; border.color: "#4a7aba" }
            }
            Button {
                text: "Создать игру"; width: 130
                onClicked: {
                    gameController.hostGame(roomNameField.text, parseInt(hostPortField.text))
                    autoJoinTimer.start()
                }
                background: Rectangle { color: parent.hovered ? "#4a8a4a" : "#2a6a2a"; radius: 5 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // Auto-join timer: connects to own server after creation
        Timer {
            id: autoJoinTimer
            interval: 300; repeat: false
            onTriggered: networkClient.connectToServer("localhost", parseInt(hostPortField.text))
        }

        // LAN server list header + Refresh button
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Text { text: "Серверы в локальной сети:"; color: "#aaa"; font.pixelSize: 13
                Layout.fillWidth: true }
            Button {
                text: lanSearchTimer.running ? "Поиск..." : "Обновить"
                width: 110; height: 28
                enabled: !lanSearchTimer.running
                onClicked: {
                    networkClient.startLanDiscovery()
                    lanSearchTimer.start()
                }
                background: Rectangle {
                    color: lanSearchTimer.running ? "#1a3a4a" : (parent.hovered ? "#3a6a9a" : "#2a4a6a")
                    radius: 4
                }
                contentItem: Text { text: parent.text; color: lanSearchTimer.running ? "#666" : "#ccc"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // LAN search stops after 7 seconds
        Timer {
            id: lanSearchTimer
            interval: 7000; repeat: false
            onTriggered: networkClient.stopLanDiscovery()
        }

        // Server list
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#1a3050"; radius: 6; clip: true

            ListView {
                anchors.fill: parent; anchors.margins: 6
                model: networkClient.lanServers; clip: true
                delegate: Rectangle {
                    width: parent.width; height: 40; radius: 4
                    color: mouseArea.containsMouse ? "#2a4a6a" : "#1a3555"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 6; spacing: 8
                        Text { text: modelData.name; color: "#eee"; font.pixelSize: 13; Layout.fillWidth: true }
                        Text { text: modelData.players + "/4"; color: "#aaa"; font.pixelSize: 12 }
                        Text { text: modelData.ip + ":" + modelData.port; color: "#888"; font.pixelSize: 11 }
                        Text { text: modelData.inGame ? "В игре" : "Лобби"
                            color: modelData.inGame ? "#ff8800" : "#44cc44"; font.pixelSize: 11 }
                    }
                    MouseArea {
                        id: mouseArea; anchors.fill: parent; hoverEnabled: true
                        onDoubleClicked: networkClient.connectToServer(modelData.ip, modelData.port)
                    }
                }

                Text {
                    anchors.centerIn: parent; visible: parent.count === 0
                    text: lanSearchTimer.running ? "Поиск серверов..." : "Нажмите «Обновить» для поиска"
                    color: "#666"; font.pixelSize: 14
                }
            }
        }

        Button {
            text: "Назад"; Layout.alignment: Qt.AlignHCenter; width: 200
            onClicked: { networkClient.stopLanDiscovery(); lanSearchTimer.stop(); gameController.showMainMenu() }
            background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 5 }
            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }
    }

    // ===== LOBBY (connected, not in game) =====
    Item {
        anchors.fill: parent
        anchors.margins: 16
        visible: inLobby && !inGame

        // Title
        Text {
            id: lobbyTitle
            text: "Лобби" + (networkClient.isHost ? " (вы хост)" : "")
            font.pixelSize: 24; font.bold: true; color: "#ffd700"
            anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        }

        // ===== LEFT HALF: Slots + Chat =====
        Item {
            id: leftPanel
            anchors.top: lobbyTitle.bottom; anchors.topMargin: 12
            anchors.left: parent.left; anchors.bottom: lobbyButtons.top
            anchors.bottomMargin: 10
            width: parent.width * 0.48

            // --- SLOTS (top of left panel) ---
            Rectangle {
                id: slotsPanel
                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                height: slotCol.height + 20
                color: "#1a3050"; radius: 6

                Column {
                    id: slotCol
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 10
                    spacing: 6

                    Text { text: "Слоты игроков"; color: "#aaa"; font.pixelSize: 12; font.bold: true }

                    Repeater {
                        model: networkClient.roomSlots

                        RowLayout {
                            width: parent.width; spacing: 8; height: 30

                            // Team color dot
                            Rectangle {
                                width: 18; height: 18; radius: 9
                                color: ["#e0e0e0", "#f0d000", "#303030", "#d03030"][index]
                                border.color: "#000"; border.width: 1
                            }

                            // Slot label
                            Text {
                                text: {
                                    var s = modelData.state
                                    if (s === "player") return modelData.name || ("Игрок " + (index+1))
                                    if (s === "ai") return "ИИ бот"
                                    if (s === "closed") return "Закрыт"
                                    return "Открыт (ожидание)"
                                }
                                color: modelData.state === "player" ? "#eee" :
                                       modelData.state === "ai" ? "#80c080" :
                                       modelData.state === "closed" ? "#666" : "#aaa"
                                font.pixelSize: 14; Layout.fillWidth: true
                            }

                            // Ready indicator
                            Text {
                                text: modelData.ready ? "\u2714 Готов" : ""
                                color: "#44ff44"; font.pixelSize: 12
                                visible: modelData.state === "player"
                            }

                            // Host: dropdown to change slot state
                            ComboBox {
                                visible: networkClient.isHost && modelData.state !== "player"
                                width: 110; height: 26
                                model: ["Открыт", "ИИ", "Закрыт"]
                                currentIndex: {
                                    if (modelData.state === "ai") return 1
                                    if (modelData.state === "closed") return 2
                                    return 0
                                }
                                onActivated: function(idx) {
                                    var states = ["open", "ai", "closed"]
                                    networkClient.setSlot(index, states[idx])
                                }
                                background: Rectangle { color: "#2a3a5a"; radius: 3; border.color: "#4a6a8a" }
                                contentItem: Text {
                                    text: parent.displayText; color: "#ccc"; font.pixelSize: 11
                                    leftPadding: 6; verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }

            // --- MAP SELECTOR (host only, below slots) ---
            Rectangle {
                id: mapPanel
                anchors.top: slotsPanel.bottom; anchors.topMargin: 8
                anchors.left: parent.left; anchors.right: parent.right
                height: networkClient.isHost ? 40 : 0; visible: networkClient.isHost
                color: "#1a3050"; radius: 6

                RowLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 8
                    Text { text: "Карта:"; color: "#aaa"; font.pixelSize: 13 }
                    ComboBox {
                        id: lobbyMapSelector
                        Layout.fillWidth: true; height: 28
                        model: {
                            var maps = gameController.availableMaps()
                            var names = []
                            for (var i = 0; i < maps.length; i++) names.push(maps[i].name)
                            return names
                        }
                        property var mapIds: {
                            var maps = gameController.availableMaps()
                            var ids = []
                            for (var i = 0; i < maps.length; i++) ids.push(maps[i].id)
                            return ids
                        }
                        currentIndex: 0
                        background: Rectangle { color: "#2a3a5a"; radius: 3; border.color: "#4a6a8a" }
                        contentItem: Text {
                            text: lobbyMapSelector.displayText; color: "#ccc"; font.pixelSize: 12
                            leftPadding: 6; verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // --- CHAT (bottom of left panel) ---
            Rectangle {
                anchors.top: mapPanel.visible ? mapPanel.bottom : slotsPanel.bottom
                anchors.topMargin: 8
                anchors.left: parent.left; anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: "#1a3050"; radius: 6; clip: true

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 4

                    Text { text: "Чат"; color: "#888"; font.pixelSize: 11; font.bold: true }

                    Flickable {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentHeight: chatText.height; clip: true

                        TextEdit {
                            id: chatText; width: parent.width
                            text: networkClient.chatMessages.join("\n")
                            color: "#b0c0d0"; font.pixelSize: 11
                            readOnly: true; wrapMode: TextEdit.WordWrap; selectByMouse: true
                        }
                        onContentHeightChanged: {
                            if (contentHeight > height) contentY = contentHeight - height
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        TextField {
                            id: chatInput; Layout.fillWidth: true
                            placeholderText: "Сообщение..."; color: "white"; font.pixelSize: 11
                            background: Rectangle { color: "#152535"; radius: 3; border.color: "#3a5a7a" }
                            onAccepted: {
                                if (text.length > 0) { networkClient.sendChat(text); text = "" }
                            }
                        }
                        Button {
                            text: ">>"; width: 36; height: 26
                            onClicked: { if (chatInput.text.length > 0) { networkClient.sendChat(chatInput.text); chatInput.text = "" } }
                            background: Rectangle { color: "#2a4a6a"; radius: 3 }
                            contentItem: Text { text: parent.text; color: "#aaa"; font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
            }
        }

        // ===== RIGHT HALF: Map preview + info =====
        Rectangle {
            id: rightPanel
            anchors.top: lobbyTitle.bottom; anchors.topMargin: 12
            anchors.left: leftPanel.right; anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.bottom: lobbyButtons.top; anchors.bottomMargin: 10
            color: "#12253a"; radius: 6

            Column {
                anchors.fill: parent; anchors.margins: 12; spacing: 8

                Text {
                    text: "Превью карты"
                    color: "#aaa"; font.pixelSize: 14; font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Map name
                Text {
                    property string selMapId: {
                        if (!lobbyMapSelector.mapIds || lobbyMapSelector.mapIds.length === 0) return "classic"
                        return lobbyMapSelector.mapIds[lobbyMapSelector.currentIndex] || "classic"
                    }
                    id: mapNameLabel
                    text: lobbyMapSelector.displayText || "Classic Island"
                    color: "#ffd700"; font.pixelSize: 16; font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Preview image
                Image {
                    id: mapPreviewImg
                    width: Math.min(parent.width - 20, parent.height - 100)
                    height: width
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: gameController.mapPreviewUrl(mapNameLabel.selMapId)
                    visible: status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    smooth: false // pixel art style
                }

                // Fallback if no preview
                Rectangle {
                    width: mapPreviewImg.width; height: mapPreviewImg.height
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !mapPreviewImg.visible
                    color: "#1a3050"; radius: 4
                    Text {
                        anchors.centerIn: parent
                        text: "Нет превью"
                        color: "#445"; font.pixelSize: 14
                    }
                }

                // Map info
                Text {
                    property var mapInfo: {
                        var maps = gameController.availableMaps()
                        var id = mapNameLabel.selMapId
                        for (var i = 0; i < maps.length; i++)
                            if (maps[i].id === id) return maps[i]
                        return null
                    }
                    text: mapInfo ? ("Суша: " + mapInfo.landCells + " клеток | Игроки: " +
                          mapInfo.minPlayers + "-" + mapInfo.maxPlayers) : ""
                    color: "#888"; font.pixelSize: 11
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // ===== BOTTOM BUTTONS =====
        RowLayout {
            id: lobbyButtons
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 10; height: 42

            Button {
                text: "Готов"; Layout.fillWidth: true
                onClicked: networkClient.toggleReady()
                background: Rectangle { color: parent.hovered ? "#3a7a3a" : "#2a5a2a"; radius: 5 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }

            Button {
                text: "Начать игру"; Layout.fillWidth: true
                visible: networkClient.isHost
                onClicked: networkClient.requestStartGame()
                background: Rectangle { color: parent.hovered ? "#6a8a3a" : "#4a6a2a"; radius: 5 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }

            Button {
                text: "Выйти"; Layout.fillWidth: true
                onClicked: { networkClient.disconnect(); gameController.showMainMenu() }
                background: Rectangle { color: parent.hovered ? "#5a3030" : "#4a2020"; radius: 5 }
                contentItem: Text { text: parent.text; color: "#faa"; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
    }
}
