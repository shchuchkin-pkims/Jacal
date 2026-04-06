import QtQuick 2.15

Item {
    id: boardRoot

    property real cellSize: Math.min(width, height) / 13

    // Board background
    Rectangle {
        anchors.centerIn: parent
        width: cellSize * 13
        height: cellSize * 13
        color: "#0d2844"
        radius: 4
    }

    // Tile grid
    Grid {
        id: tileGrid
        anchors.centerIn: parent
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
                    ("file://" + gameController.assetsPath + "/ship_" + (modelData.team + 1) + ".png") : ""
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

    // Pirates layer — BRIGHT, CONTRASTING tokens
    Repeater {
        model: gameController.pirates

        Item {
            id: pirateItem
            property int pirateIdx: {
                var count = 0;
                var myIdx = 0;
                var pList = gameController.pirates;
                for (var i = 0; i < pList.length; i++) {
                    if (pList[i].row === modelData.row && pList[i].col === modelData.col) {
                        if (i === index) myIdx = count;
                        count++;
                    }
                }
                return myIdx;
            }

            x: tileGrid.x + modelData.col * cellSize + cellSize * 0.08 + pirateIdx * cellSize * 0.25
            y: tileGrid.y + modelData.row * cellSize + cellSize * 0.5
            width: cellSize * 0.35
            height: cellSize * 0.42

            Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
            Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

            // Shadow
            Rectangle {
                anchors.fill: body
                anchors.topMargin: 2
                anchors.leftMargin: 2
                radius: body.radius
                color: "#60000000"
            }

            // Pirate body
            Rectangle {
                id: body
                anchors.fill: parent
                radius: 5
                opacity: modelData.trapped ? 0.6 : 1.0

                // Vivid fill colors
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: {
                            switch(modelData.color) {
                                case "#e0e0e0": return "#ffffff";   // White: bright white top
                                case "#f0d000": return "#ffee33";   // Yellow: bright yellow
                                case "#303030": return "#555555";   // Black: dark gray top
                                case "#d03030": return "#ff4444";   // Red: bright red
                                default: return modelData.color;
                            }
                        }
                    }
                    GradientStop {
                        position: 1.0
                        color: {
                            switch(modelData.color) {
                                case "#e0e0e0": return "#b0b0c0";   // White: grayish bottom
                                case "#f0d000": return "#cc9900";   // Yellow: dark gold
                                case "#303030": return "#111111";   // Black: near-black
                                case "#d03030": return "#881111";   // Red: dark red
                                default: return modelData.color;
                            }
                        }
                    }
                }

                // Bold contrasting border
                border.width: modelData.selected ? 3.5 : 2.5
                border.color: {
                    if (modelData.selected) return "#ffff00";
                    if (modelData.isCurrentTeam) return "#ffffff";
                    switch(modelData.color) {
                        case "#e0e0e0": return "#333333";  // White pirate: dark border
                        case "#303030": return "#cccccc";  // Black pirate: light border
                        default: return "#000000";
                    }
                }

                // Team letter
                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: modelData.hasCoin ? -parent.height * 0.12 : 0
                    text: {
                        switch(modelData.color) {
                            case "#e0e0e0": return "W";
                            case "#f0d000": return "Y";
                            case "#303030": return "B";
                            case "#d03030": return "R";
                            default: return "?";
                        }
                    }
                    color: {
                        switch(modelData.color) {
                            case "#e0e0e0": return "#222222";
                            case "#303030": return "#eeeeee";
                            default: return "#000000";
                        }
                    }
                    font.pixelSize: parent.height * 0.38
                    font.bold: true
                    style: Text.Outline
                    styleColor: modelData.color === "#303030" ? "#00000040" : "#ffffff40"
                }

                // Coin indicator
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: parent.width * 0.5
                    height: parent.height * 0.25
                    radius: 3
                    color: "#ffd700"
                    border.color: "#aa8800"
                    border.width: 1
                    visible: modelData.hasCoin

                    Text {
                        anchors.centerIn: parent
                        text: "$"
                        color: "#553300"
                        font.pixelSize: parent.height * 0.75
                        font.bold: true
                    }
                }

                // Selection glow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3
                    radius: parent.radius + 3
                    color: "transparent"
                    border.color: "#ffff00"
                    border.width: 2
                    visible: modelData.selected

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: modelData.selected
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
