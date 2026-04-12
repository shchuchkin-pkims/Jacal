import QtQuick 2.15

Item {
    id: boardRoot

    property real labelSize: 14
    property real cellSize: Math.min(width - labelSize - 4, height - labelSize - 4) / 13

    // Board background
    Rectangle {
        anchors.centerIn: parent
        width: cellSize * 13 + labelSize + 4
        height: cellSize * 13 + labelSize + 4
        color: "#0d2844"
        radius: 4
    }

    // Y-axis labels (left side, chess-style: 1 at bottom, 13 at top)
    Repeater {
        model: 13
        Text {
            x: tileGrid.x - labelSize - 2
            y: tileGrid.y + index * cellSize + cellSize / 2 - height / 2
            text: (13 - index).toString()
            color: "#667788"
            font.pixelSize: Math.min(labelSize - 2, cellSize * 0.35)
            font.bold: true
            horizontalAlignment: Text.AlignRight
            width: labelSize
        }
    }

    // X-axis labels (bottom, chess-style: a at left, m at right)
    Repeater {
        model: 13
        Text {
            x: tileGrid.x + index * cellSize + cellSize / 2 - width / 2
            y: tileGrid.y + 13 * cellSize + 2
            text: String.fromCharCode(97 + index)  // 'a' + index
            color: "#667788"
            font.pixelSize: Math.min(labelSize - 2, cellSize * 0.35)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Tile grid
    Grid {
        id: tileGrid
        x: parent.width / 2 - (13 * cellSize) / 2 + labelSize / 2
        y: parent.height / 2 - (13 * cellSize) / 2 - labelSize / 2
        rows: 13
        columns: 13

        Repeater {
            model: boardModel
            TileItem {
                width: boardRoot.cellSize
                height: boardRoot.cellSize
            }
        }
    }

    // Ships layer — using tile images from PDF
    Repeater {
        model: gameController.shipList

        Item {
            x: tileGrid.x + modelData.col * cellSize
            y: tileGrid.y + modelData.row * cellSize
            width: cellSize
            height: cellSize

            // Ship image from extracted PDF tiles
            Image {
                id: shipImg
                anchors.fill: parent
                anchors.margins: 1
                source: gameController.assetsPath !== "" ?
                    (gameController.fileUrl("ship_" + (modelData.team + 1) + ".png")) : ""
                visible: status === Image.Ready
                fillMode: Image.PreserveAspectFit
                smooth: true
                // Rotate ship to face the island (inward)
                rotation: {
                    if (modelData.row === 0)  return 180; // North ship faces South
                    if (modelData.row === 12) return 0;   // South ship faces North
                    if (modelData.col === 0)  return 90;  // West ship faces East
                    if (modelData.col === 12) return -90; // East ship faces West
                    return 0;
                }
                opacity: modelData.isCurrentTeam ? 1.0 : 0.7
            }

            // Fallback if no image
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 5
                color: Qt.rgba(0, 0, 0, 0.3)
                border.color: modelData.color
                border.width: modelData.isCurrentTeam ? 3 : 2
                visible: !shipImg.visible
                Text {
                    anchors.centerIn: parent
                    text: "\u26F5"
                    font.pixelSize: parent.width * 0.5
                }
            }

            // Team color border overlay
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: modelData.color
                border.width: modelData.isCurrentTeam ? 3 : 1.5
                radius: 3
                opacity: 0.8
            }

            // Pirate count badge
            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 1
                width: pirateCountText.width + 6
                height: pirateCountText.height + 4
                radius: 4
                color: modelData.color
                border.color: "#000000"
                border.width: 1
                visible: modelData.piratesOnBoard > 0

                Text {
                    id: pirateCountText
                    anchors.centerIn: parent
                    text: modelData.piratesOnBoard.toString()
                    color: (modelData.color === "#303030") ? "white" : "black"
                    font.pixelSize: parent.parent.width * 0.22
                    font.bold: true
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: gameController.cellClicked(modelData.row, modelData.col)
            }
        }
    }

    // Pirates layer — Portrait-based tokens
    Repeater {
        model: gameController.pirates

        Item {
            id: pirateItem
            property int pirateIdx: {
                var count = 0; var myIdx = 0;
                var pList = gameController.pirates;
                for (var i = 0; i < pList.length; i++) {
                    if (pList[i].row === modelData.row && pList[i].col === modelData.col) {
                        if (i === index) myIdx = count;
                        count++;
                    }
                }
                return myIdx;
            }

            property bool isSelected: modelData.selected
            property real baseSize: cellSize * 0.36
            property real selSize: cellSize * 0.50

            x: tileGrid.x + modelData.col * cellSize + cellSize * 0.05 + pirateIdx * cellSize * 0.20
            y: tileGrid.y + modelData.row * cellSize + (isSelected ? cellSize * 0.30 : cellSize * 0.46)
            width: isSelected ? selSize : baseSize
            height: isSelected ? selSize * 1.15 : baseSize * 1.15
            z: isSelected ? 100 : pirateIdx  // Selected pirate on top

            Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
            Behavior on width { NumberAnimation { duration: 150 } }
            Behavior on height { NumberAnimation { duration: 150 } }

            // Shadow
            Rectangle {
                x: 2; y: 2; width: parent.width; height: parent.height
                radius: 4; color: "#50000000"
            }

            // Portrait frame
            Rectangle {
                id: frame
                anchors.fill: parent
                radius: 4
                color: "#20000000"
                opacity: modelData.trapped ? 0.5 : 1.0

                // Border: team color for characters, selection glow for selected
                border.width: isSelected ? 3 : (modelData.isCharacter ? 2.5 : 2)
                border.color: {
                    if (isSelected) return "#ffff00"
                    if (modelData.isCharacter) return modelData.color
                    if (modelData.isCurrentTeam) return "#ffffffcc"
                    return modelData.color
                }

                // Portrait image
                Image {
                    id: portraitImg
                    anchors.fill: parent
                    anchors.margins: 1.5
                    source: {
                        if (!modelData.portrait || modelData.portrait === "") return ""
                        var folder = modelData.isCharacter ? "" : "../portraits/"
                        if (modelData.isCharacter) folder = "../portraits/"
                        return gameController.fileUrl(folder + modelData.portrait)
                    }
                    visible: status === Image.Ready
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                }

                // Fallback: team letter (if portrait not found)
                Text {
                    anchors.centerIn: parent
                    visible: !portraitImg.visible
                    text: {
                        if (modelData.isCharacter) return "?"
                        if (modelData.color === "#e0e0e0") return "W"
                        if (modelData.color === "#f0d000") return "Y"
                        if (modelData.color === "#303030") return "B"
                        if (modelData.color === "#d03030") return "R"
                        return "?"
                    }
                    color: modelData.color === "#303030" ? "#eee" : "#222"
                    font.pixelSize: parent.height * 0.4
                    font.bold: true
                }

                // Coin image (replaces $ rectangle)
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -2
                    width: parent.width * 0.55
                    height: width
                    source: modelData.hasCoin ? gameController.fileUrl("coin.png") : ""
                    visible: modelData.hasCoin && status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                // Coin fallback text
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: "$"
                    color: "#ffd700"
                    font.pixelSize: parent.height * 0.3
                    font.bold: true
                    visible: modelData.hasCoin && !portraitImg.visible
                    style: Text.Outline; styleColor: "#000"
                }

                // Selection glow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3
                    radius: parent.radius + 3
                    color: "transparent"
                    border.color: "#ffff00"
                    border.width: 2
                    visible: isSelected
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite; running: isSelected
                        NumberAnimation { from: 0.4; to: 1.0; duration: 400 }
                        NumberAnimation { from: 1.0; to: 0.4; duration: 400 }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: gameController.cellClicked(modelData.row, modelData.col)
            }
        }
    }

    // Valid move highlights
    Repeater {
        model: gameController.validTargets

        Rectangle {
            x: tileGrid.x + modelData.col * cellSize
            y: tileGrid.y + modelData.row * cellSize
            width: cellSize
            height: cellSize
            color: "#3300ff00"
            border.color: "#80ff80"
            border.width: 2
            radius: 3

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { from: 0.5; to: 1.0; duration: 600 }
                NumberAnimation { from: 1.0; to: 0.5; duration: 600 }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: gameController.cellClicked(modelData.row, modelData.col)
            }
        }
    }
}
