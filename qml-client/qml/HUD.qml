import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#152840"
    radius: 6
    border.color: "#2a4a6a"
    border.width: 1

    property bool showLog: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Current turn
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: gameController.isAITurn ? "#302020" : "#1a3050"
            radius: 6

            Column {
                anchors.centerIn: parent
                spacing: 2
                Text {
                    text: "Ход " + gameController.turnNumber
                    color: "#aaa"; font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: gameController.currentTeamName + (gameController.isAITurn ? " (ИИ)" : "")
                    color: gameController.currentTeamColor
                    font.pixelSize: 22; font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                height: 3; color: "#ff8800"; visible: gameController.isAITurn; radius: 1
                SequentialAnimation on opacity {
                    loops: Animation.Infinite; running: gameController.isAITurn
                    NumberAnimation { from: 0.3; to: 1.0; duration: 400 }
                    NumberAnimation { from: 1.0; to: 0.3; duration: 400 }
                }
            }
        }

        // Status
        Text {
            Layout.fillWidth: true
            text: gameController.phaseText !== "" ? gameController.phaseText : gameController.statusText
            color: gameController.phaseText !== "" ? "#ffaa00" :
                   (gameController.isAITurn ? "#ff8800" : "#ccc")
            font.pixelSize: 12; wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        // Ship controls
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            visible: gameController.gameActive && !gameController.isAITurn
            Button {
                text: "\u25C0 Корабль"; Layout.fillWidth: true
                onClicked: gameController.moveShipLeft()
                background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                text: "Корабль \u25B6"; Layout.fillWidth: true
                onClicked: gameController.moveShipRight()
                background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // Tab: Scores / Log
        Row {
            Layout.fillWidth: true; spacing: 6
            Button {
                text: "Счёт"; width: parent.width / 2 - 3; height: 28
                onClicked: showLog = false
                background: Rectangle { color: !showLog ? "#3a5a7a" : "#1a3050"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                text: "Лог ходов"; width: parent.width / 2 - 3; height: 28
                onClicked: showLog = true
                background: Rectangle { color: showLog ? "#3a5a7a" : "#1a3050"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // === SCORES TAB ===
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#1a3050"; radius: 6; visible: !showLog; clip: true

            Column {
                anchors.fill: parent; anchors.margins: 8; spacing: 6

                Repeater {
                    model: gameController.scores
                    RowLayout {
                        width: parent.width; spacing: 6
                        Rectangle { width: 12; height: 12; radius: 6; color: modelData.color }
                        Text { text: modelData.team + (modelData.isAI ? " (ИИ)" : "")
                            color: "#ccc"; font.pixelSize: 13; Layout.fillWidth: true }
                        Text { text: modelData.score.toString()
                            color: "#ffd700"; font.pixelSize: 16; font.bold: true }
                        Text { text: "(" + modelData.alive + ")"
                            color: "#888"; font.pixelSize: 11 }
                    }
                }

                Item { width: 1; height: 10 }

                Text { text: "Обозначения"; color: "#888"; font.pixelSize: 12; font.bold: true }
                Text { text: "> стрелка  L конь  ** лёд"; color: "#666"; font.pixelSize: 10 }
                Text { text: "## ловушка  XX людоед  <> крокодил"; color: "#666"; font.pixelSize: 10 }
                Text { text: "$ сундук  BL шар  CN пушка  CV пещера"; color: "#666"; font.pixelSize: 10 }
                Text { text: "J/D/S/M вертушки  [] крепость"; color: "#666"; font.pixelSize: 10 }
            }
        }

        // === LOG TAB (selectable, copyable) ===
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#1a3050"; radius: 6; visible: showLog; clip: true

            Flickable {
                id: logFlick
                anchors.fill: parent; anchors.margins: 6
                contentWidth: logArea.width; contentHeight: logArea.height
                clip: true

                TextEdit {
                    id: logArea
                    width: logFlick.width
                    text: gameController.moveLog.join("\n")
                    color: "#b0c8e0"
                    font.pixelSize: 11; font.family: "monospace"
                    readOnly: true
                    selectByMouse: true
                    selectedTextColor: "#000000"
                    selectionColor: "#60a0ff"
                    wrapMode: TextEdit.WordWrap
                }

                onContentHeightChanged: {
                    if (contentHeight > height)
                        contentY = contentHeight - height
                }
            }

            // Copy button
            Button {
                anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 4
                width: 60; height: 22
                text: "Copy"
                font.pixelSize: 10
                onClicked: {
                    logArea.selectAll()
                    logArea.copy()
                    logArea.deselect()
                    copyHint.visible = true
                    copyTimer.restart()
                }
                background: Rectangle { color: parent.hovered ? "#4a6a8a" : "#2a4a6a"; radius: 3 }
                contentItem: Text { text: parent.text; color: "#aaa"; font: parent.font
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Text {
                id: copyHint; anchors.right: parent.right; anchors.top: parent.top
                anchors.topMargin: 28; anchors.rightMargin: 4
                text: "Copied!"; color: "#80ff80"; font.pixelSize: 10; visible: false
            }
            Timer { id: copyTimer; interval: 1500; onTriggered: copyHint.visible = false }
        }

        // Bottom buttons
        RowLayout {
            Layout.fillWidth: true; spacing: 6

            Button {
                text: "Отмена"; Layout.fillWidth: true
                visible: gameController.hasSelection && !gameController.isAITurn
                onClicked: gameController.cancelSelection()
                background: Rectangle { color: parent.hovered ? "#5a3030" : "#4a2020"; radius: 4 }
                contentItem: Text { text: parent.text; color: "#faa"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                text: "В меню"; Layout.fillWidth: true
                visible: gameController.gameActive
                onClicked: gameController.quitToMenu()
                background: Rectangle { color: parent.hovered ? "#4a4a20" : "#3a3a10"; radius: 4 }
                contentItem: Text { text: parent.text; color: "#cc8"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
    }
}
