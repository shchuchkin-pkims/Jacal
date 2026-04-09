import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#152840"; radius: 6
    border.color: "#2a4a6a"; border.width: 1

    property bool showLog: false

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 8; spacing: 5

        // === Turn header ===
        Rectangle {
            Layout.fillWidth: true; height: 46
            color: gameController.isAITurn ? "#302020" : "#1a3050"; radius: 6
            RowLayout {
                anchors.fill: parent; anchors.margins: 5; spacing: 6
                Column {
                    Layout.fillWidth: true
                    Text { text: "Ход " + gameController.turnNumber; color: "#aaa"; font.pixelSize: 10 }
                    Text {
                        text: gameController.currentTeamName + (gameController.isAITurn ? " (ИИ)" : "")
                        color: gameController.currentTeamColor; font.pixelSize: 17; font.bold: true
                    }
                }
                // Rum button (clickable to use rum)
                Rectangle {
                    visible: gameController.currentTeamRum > 0
                    width: rumRow.width + 8; height: 24; radius: 4
                    color: gameController.rumUseMode ? "#604020" :
                           (rumMA.containsMouse ? "#4a3a2a" : "transparent")
                    border.color: gameController.rumUseMode ? "#ffaa00" : "transparent"
                    border.width: gameController.rumUseMode ? 2 : 0

                    Row {
                        id: rumRow; anchors.centerIn: parent; spacing: 2
                        Image {
                            source: gameController.assetsPath !== "" ?
                                (gameController.fileUrl("rum_bottle_icon.png")) : ""
                            width: 18; height: 18; fillMode: Image.PreserveAspectFit; smooth: true
                        }
                        Text { text: "x" + gameController.currentTeamRum
                            color: "#ffd700"; font.pixelSize: 13; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter }
                    }
                    MouseArea {
                        id: rumMA; anchors.fill: parent; hoverEnabled: true
                        onClicked: {
                            if (gameController.rumUseMode) gameController.cancelRumUse()
                            else gameController.activateRumUse()
                        }
                    }
                    ToolTip {
                        visible: rumMA.containsMouse && !gameController.rumUseMode
                        text: "Использовать ром"
                        delay: 500
                    }
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

        // === Status text ===
        Text {
            Layout.fillWidth: true
            text: gameController.phaseText !== "" ? gameController.phaseText : gameController.statusText
            color: gameController.phaseText !== "" ? "#ffaa00" :
                   (gameController.isAITurn ? "#ff8800" : "#ccc")
            font.pixelSize: 10; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
        }

        // === Ship controls ===
        RowLayout {
            Layout.fillWidth: true; spacing: 4
            visible: gameController.gameActive && !gameController.isAITurn
            Button {
                text: "\u25C0 Корабль"; Layout.fillWidth: true; height: 26
                onClicked: gameController.moveShipLeft()
                background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                text: "Корабль \u25B6"; Layout.fillWidth: true; height: 26
                onClicked: gameController.moveShipRight()
                background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // === Tabs: Crew / Log ===
        Row {
            Layout.fillWidth: true; spacing: 4
            Button {
                width: parent.width / 2 - 2; height: 24; text: "Экипаж"; font.pixelSize: 10
                onClicked: showLog = false
                background: Rectangle { color: !showLog ? "#3a5a7a" : "#1a3050"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font: parent.font
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                width: parent.width / 2 - 2; height: 24; text: "Лог"; font.pixelSize: 10
                onClicked: showLog = true
                background: Rectangle { color: showLog ? "#3a5a7a" : "#1a3050"; radius: 4 }
                contentItem: Text { text: parent.text; color: "white"; font: parent.font
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // === SCORES (always visible, fixed height) ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: scoresCol.height + 10
            Layout.minimumHeight: 40
            color: "#1a3050"; radius: 5

            Column {
                id: scoresCol
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top; anchors.margins: 4; spacing: 2
                Repeater {
                    model: gameController.scores
                    RowLayout {
                        width: parent.width; spacing: 4; height: 18
                        Rectangle { width: 10; height: 10; radius: 5; color: modelData.color }
                        Text { text: modelData.team; color: "#bbb"; font.pixelSize: 11; Layout.fillWidth: true }
                        Text { text: modelData.score.toString(); color: "#ffd700"; font.pixelSize: 13; font.bold: true }
                        Text { text: "(" + modelData.alive + ")"; color: "#777"; font.pixelSize: 10 }
                    }
                }
            }
        }

        // === CREW TAB (below scores) ===
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#1a3050"; radius: 5; clip: true; visible: !showLog

            Flickable {
                anchors.fill: parent; anchors.margins: 3
                contentHeight: crewCol.height; clip: true

                Column {
                    id: crewCol; width: parent.width; spacing: 2

                    Repeater {
                        model: gameController.crewStatus
                        Rectangle {
                            width: parent.width
                            height: modelData.isCurrentTeam ? 38 : 24
                            radius: 3
                            color: {
                                if (modelData.selected) return "#2a5040"
                                if (modelData.isCurrentTeam) return "#1e3858"
                                return "#152535"
                            }
                            border.color: modelData.selected ? "#60ff60" : modelData.teamColor
                            border.width: modelData.selected ? 2 : (modelData.isCurrentTeam ? 1.5 : 0.5)
                            opacity: modelData.state === 2 ? 0.4 : 1.0

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 2; spacing: 3

                                Image {
                                    Layout.preferredWidth: modelData.isCurrentTeam ? 32 : 18
                                    Layout.preferredHeight: Layout.preferredWidth
                                    source: {
                                        if (!modelData.isCurrentTeam) return ""
                                        var pp = gameController.portraitsPath
                                        if (pp === "") return ""
                                        return gameController.fileUrl("../portraits/" + modelData.portrait)
                                    }
                                    visible: modelData.isCurrentTeam && status === Image.Ready
                                    fillMode: Image.PreserveAspectCrop; smooth: true
                                    Rectangle {
                                        anchors.fill: parent; color: "transparent"
                                        border.color: modelData.selected ? "#60ff60" : modelData.teamColor
                                        border.width: modelData.selected ? 2 : 1; radius: 2
                                    }
                                }
                                Rectangle {
                                    Layout.preferredWidth: 8; Layout.preferredHeight: 8
                                    radius: 4; color: modelData.teamColor
                                    visible: !modelData.isCurrentTeam
                                }
                                Column {
                                    Layout.fillWidth: true; spacing: 0
                                    Text {
                                        text: modelData.name
                                        color: modelData.selected ? "#80ff80" :
                                               (modelData.isCurrentTeam ? "#e0e0e0" : "#999")
                                        font.pixelSize: modelData.isCurrentTeam ? 11 : 9
                                        font.bold: modelData.isCurrentTeam || modelData.selected
                                        elide: Text.ElideRight; width: parent.width
                                    }
                                    Text {
                                        text: modelData.status
                                        color: modelData.state === 2 ? "#ff5555" :
                                               modelData.state === 3 ? "#ffaa00" :
                                               modelData.state === 4 ? "#888" : "#80c0ff"
                                        font.pixelSize: modelData.isCurrentTeam ? 9 : 8
                                    }
                                }
                                Image {
                                    Layout.preferredWidth: 14; Layout.preferredHeight: 14
                                    source: gameController.assetsPath !== "" ?
                                        (gameController.fileUrl("coin.png")) : ""
                                    visible: modelData.hasCoin
                                    fillMode: Image.PreserveAspectFit; smooth: true
                                }
                            }

                            // Click to select this crew member
                            MouseArea {
                                anchors.fill: parent
                                enabled: modelData.isCurrentTeam && modelData.state !== 2
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: gameController.selectCrewMember(modelData.teamIdx, modelData.unitIndex)
                            }
                        }
                    }
                }
            }
        }

        // === LOG TAB ===
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#1a3050"; radius: 5; visible: showLog; clip: true

            Flickable {
                id: logFlick
                anchors.fill: parent; anchors.margins: 4
                contentWidth: logArea.width; contentHeight: logArea.height; clip: true
                TextEdit {
                    id: logArea; width: logFlick.width
                    text: gameController.moveLog.join("\n")
                    color: "#b0c8e0"; font.pixelSize: 10; font.family: "monospace"
                    readOnly: true; selectByMouse: true
                    selectedTextColor: "#000"; selectionColor: "#60a0ff"
                    wrapMode: TextEdit.WordWrap
                }
                onContentHeightChanged: { if (contentHeight > height) contentY = contentHeight - height }
            }
            Button {
                anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 3
                width: 44; height: 18; text: "Copy"; font.pixelSize: 8
                onClicked: { logArea.selectAll(); logArea.copy(); logArea.deselect() }
                background: Rectangle { color: parent.hovered ? "#4a6a8a" : "#2a4a6a"; radius: 3 }
                contentItem: Text { text: parent.text; color: "#aaa"; font: parent.font
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        // === Bottom buttons ===
        RowLayout {
            Layout.fillWidth: true; spacing: 4
            Button {
                text: "Отмена"; Layout.fillWidth: true; height: 24
                visible: gameController.hasSelection && !gameController.isAITurn
                onClicked: gameController.cancelSelection()
                background: Rectangle { color: parent.hovered ? "#5a3030" : "#4a2020"; radius: 4 }
                contentItem: Text { text: parent.text; color: "#faa"; font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Button {
                text: "В меню"; Layout.fillWidth: true; height: 24
                visible: gameController.gameActive
                onClicked: gameController.quitToMenu()
                background: Rectangle { color: parent.hovered ? "#3a5a7a" : "#2a3a5a"; radius: 4 }
                contentItem: Text { text: parent.text; color: "#aac"; font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
    }
}
