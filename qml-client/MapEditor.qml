import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: editorRoot
    color: "#0e2240"

    property int gridSize: 13
    property real cellSize: Math.min((width - toolPanelWidth - 40) / gridSize,
                                      (height - 80) / gridSize)
    property int toolPanelWidth: 200
    property int activeTool: 0 // 0=Sea, 1=Land, 2=Rock, 3=ShipSpawn
    property int activeShipTeam: 0

    // Map data stored as a flat array
    property var terrainData: []
    property var shipSpawns: [] // [{team, row, col}, ...]
    property string mapName: "New Map"

    Component.onCompleted: resetMap()

    function resetMap() {
        var d = []
        for (var i = 0; i < gridSize * gridSize; i++) d.push(0) // all sea
        terrainData = d
        shipSpawns = []
    }

    function setCell(row, col, type) {
        var idx = row * gridSize + col
        var d = terrainData.slice()
        d[idx] = type
        terrainData = d
    }

    function getCell(row, col) {
        return terrainData[row * gridSize + col] || 0
    }

    function addShip(team, row, col) {
        var ships = shipSpawns.slice()
        // Remove existing ship of same team
        ships = ships.filter(function(s) { return s.team !== team })
        ships.push({team: team, row: row, col: col})
        shipSpawns = ships
    }

    function shipAt(row, col) {
        for (var i = 0; i < shipSpawns.length; i++)
            if (shipSpawns[i].row === row && shipSpawns[i].col === col)
                return shipSpawns[i].team
        return -1
    }

    function countLand() {
        var n = 0
        for (var i = 0; i < terrainData.length; i++)
            if (terrainData[i] === 1) n++
        return n
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // ===== LEFT: Grid =====
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            // Axis labels Y (left, bottom-up)
            Repeater {
                model: gridSize
                Text {
                    x: gridArea.x - 16
                    y: gridArea.y + (gridSize - 1 - index) * cellSize + cellSize / 2 - 6
                    text: index.toString()
                    color: "#556677"; font.pixelSize: 10
                }
            }
            // Axis labels X (bottom)
            Repeater {
                model: gridSize
                Text {
                    x: gridArea.x + index * cellSize + cellSize / 2 - 4
                    y: gridArea.y + gridSize * cellSize + 2
                    text: index.toString()
                    color: "#556677"; font.pixelSize: 10
                }
            }

            Grid {
                id: gridArea
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: -toolPanelWidth / 2
                rows: gridSize
                columns: gridSize

                Repeater {
                    model: gridSize * gridSize

                    Rectangle {
                        property int cellRow: Math.floor(index / gridSize)
                        property int cellCol: index % gridSize
                        property int cellType: terrainData[index] || 0
                        property int shipTeam: shipAt(cellRow, cellCol)

                        width: cellSize; height: cellSize
                        color: cellType === 0 ? "#1a6090" :
                               cellType === 1 ? "#2d5a1e" :
                               cellType === 2 ? "#6a5a4a" : "#1a6090"
                        border.color: "#ffffff20"
                        border.width: 0.5

                        // Ship marker
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.6; height: parent.height * 0.6
                            radius: width / 2
                            visible: shipTeam >= 0
                            color: ["#e0e0e0", "#f0d000", "#303030", "#d03030"][shipTeam] || "#888"
                            border.color: "white"; border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "S" + shipTeam
                                color: shipTeam === 2 ? "white" : "black"
                                font.pixelSize: parent.width * 0.4; font.bold: true
                            }
                        }

                        // Terrain icon
                        Text {
                            anchors.centerIn: parent
                            visible: shipTeam < 0
                            text: cellType === 0 ? "~" : cellType === 2 ? "X" : ""
                            color: cellType === 0 ? "#3090c0" : "#8a7a6a"
                            font.pixelSize: parent.width * 0.4
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (activeTool === 3) {
                                    // Place ship spawn (must be on water)
                                    if (getCell(cellRow, cellCol) === 0)
                                        addShip(activeShipTeam, cellRow, cellCol)
                                } else {
                                    setCell(cellRow, cellCol, activeTool)
                                }
                            }
                            onPositionChanged: {
                                if (pressed && activeTool < 3) {
                                    setCell(cellRow, cellCol, activeTool)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ===== RIGHT: Tool Panel =====
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: toolPanelWidth
            color: "#152840"
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Редактор карт"
                    font.pixelSize: 18; font.bold: true; color: "#ffd700"
                    Layout.alignment: Qt.AlignHCenter
                }

                // Map name
                TextField {
                    id: mapNameField
                    Layout.fillWidth: true; height: 32
                    text: mapName; color: "white"; font.pixelSize: 13
                    placeholderText: "Имя карты"
                    background: Rectangle { color: "#1a3a5a"; radius: 4; border.color: "#4a7aba" }
                    onTextChanged: mapName = text
                }

                // Stats
                Text {
                    text: "Суша: " + countLand() + " клеток\nКорабли: " + shipSpawns.length
                    color: "#aaa"; font.pixelSize: 11
                }

                // Tool selector
                Text { text: "Инструменты:"; color: "#ccc"; font.pixelSize: 13; font.bold: true }

                // Sea
                Button {
                    Layout.fillWidth: true; height: 36
                    text: "~ Море (вода)"
                    onClicked: activeTool = 0
                    background: Rectangle {
                        color: activeTool === 0 ? "#2070a0" : "#1a3a5a"; radius: 4
                        border.color: activeTool === 0 ? "#60b0ff" : "#4a7aba"; border.width: activeTool === 0 ? 2 : 1
                    }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                // Land
                Button {
                    Layout.fillWidth: true; height: 36
                    text: "# Суша"
                    onClicked: activeTool = 1
                    background: Rectangle {
                        color: activeTool === 1 ? "#2a6a1e" : "#1a3a5a"; radius: 4
                        border.color: activeTool === 1 ? "#60ff60" : "#4a7aba"; border.width: activeTool === 1 ? 2 : 1
                    }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                // Rock
                Button {
                    Layout.fillWidth: true; height: 36
                    text: "X Скала"
                    onClicked: activeTool = 2
                    background: Rectangle {
                        color: activeTool === 2 ? "#6a5a3a" : "#1a3a5a"; radius: 4
                        border.color: activeTool === 2 ? "#c0a060" : "#4a7aba"; border.width: activeTool === 2 ? 2 : 1
                    }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                // Ship spawn
                Text { text: "Точки появления:"; color: "#ccc"; font.pixelSize: 12; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 4
                    Repeater {
                        model: [
                            {label: "W", color: "#e0e0e0", team: 0},
                            {label: "Y", color: "#f0d000", team: 1},
                            {label: "B", color: "#303030", team: 2},
                            {label: "R", color: "#d03030", team: 3}
                        ]
                        Button {
                            Layout.fillWidth: true; height: 32
                            text: modelData.label
                            onClicked: { activeTool = 3; activeShipTeam = modelData.team }
                            background: Rectangle {
                                color: (activeTool === 3 && activeShipTeam === modelData.team) ?
                                    modelData.color : "#1a3a5a"
                                radius: 4; border.color: modelData.color; border.width: 2
                            }
                            contentItem: Text { text: parent.text
                                color: (modelData.team === 2) ? "white" : "black"
                                font.pixelSize: 12; font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
                Text { text: "Клик на воду = установить\nточку появления корабля"
                    color: "#888"; font.pixelSize: 9; wrapMode: Text.WordWrap }

                Item { Layout.fillHeight: true }

                // Save button
                Button {
                    Layout.fillWidth: true; height: 40
                    text: "Сохранить карту"
                    enabled: mapName.length > 0 && countLand() > 0 && shipSpawns.length >= 2
                    onClicked: {
                        var terrainStr = ""
                        for (var r = 0; r < gridSize; r++) {
                            for (var c = 0; c < gridSize; c++) {
                                var t = getCell(r, c)
                                terrainStr += (t === 0 ? "." : t === 1 ? "#" : "X")
                            }
                            terrainStr += "\n"
                        }
                        var shipsStr = ""
                        for (var i = 0; i < shipSpawns.length; i++) {
                            var s = shipSpawns[i]
                            shipsStr += s.team + "," + s.row + "," + s.col + ";"
                        }
                        gameController.saveCustomMap(mapName, terrainStr, shipsStr)
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#3a8a3a" : "#2a6a2a") : "#1a3a2a"
                        radius: 5
                    }
                    contentItem: Text { text: parent.text
                        color: parent.enabled ? "white" : "#555"
                        font.pixelSize: 14; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Text {
                    visible: countLand() === 0 || shipSpawns.length < 2
                    text: countLand() === 0 ? "Нужна хотя бы 1 клетка суши" :
                          "Нужно минимум 2 точки появления"
                    color: "#ff8888"; font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Clear
                Button {
                    Layout.fillWidth: true; height: 32
                    text: "Очистить"
                    onClicked: resetMap()
                    background: Rectangle { color: parent.hovered ? "#5a3030" : "#3a2020"; radius: 4 }
                    contentItem: Text { text: parent.text; color: "#faa"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }

                // Back
                Button {
                    Layout.fillWidth: true; height: 32
                    text: "\u25C0  Назад"
                    onClicked: gameController.showMainMenu()
                    background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                    contentItem: Text { text: parent.text; color: "#aaa"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }
    }
}
