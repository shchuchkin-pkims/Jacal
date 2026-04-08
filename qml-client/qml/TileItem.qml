import QtQuick 2.15

Rectangle {
    id: tileRoot

    color: cellColor
    border.color: borderColor
    border.width: 0.5
    radius: 1

    // Tile image (rotated for arrows/cannons to match actual direction)
    Image {
        id: tileImage
        anchors.fill: parent
        anchors.margins: 0.5
        source: imageSource !== "" ? imageSource : ""
        visible: imageSource !== "" && status === Image.Ready
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        // Rotate tile image to match actual game direction
        rotation: imageRotation
        transformOrigin: Item.Center
    }

    // Fallback tile symbol (when no image)
    Text {
        anchors.centerIn: parent
        text: tileSymbol
        color: {
            switch(tileTypeName) {
                case "Cannibal": return "#ff4040";
                case "Trap": return "#ffaa00";
                case "Ice": return "#60c0ff";
                case "Fortress": case "ResurrectFort": return "#ffffff";
                case "Balloon": return "#e0e0ff";
                case "Cave": return "#aaaaaa";
                default: return "#f0f0e0";
            }
        }
        font.pixelSize: tileRoot.width * 0.32
        font.bold: true
        visible: isLand && !tileImage.visible && revealed && spinnerSteps === 0
        style: Text.Outline
        styleColor: "#000000"
    }

    // Horse tile: just show the image, no overlay

    // === ARROW DIRECTION OVERLAY (Unicode arrows, always visible on arrow tiles) ===
    Item {
        anchors.fill: parent
        visible: tileTypeName === "Arrow" && revealed

        Repeater {
            model: 8
            Text {
                property bool active: (directionBits & (1 << index)) !== 0
                visible: active
                text: ["\u2B06", "\u2197", "\u27A1", "\u2198",
                       "\u2B07", "\u2199", "\u2B05", "\u2196"][index]
                color: "#ffee00"
                font.pixelSize: tileRoot.width * 0.3
                font.bold: true
                style: Text.Outline
                styleColor: "#000000"
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: [0, 1, 1, 1, 0, -1, -1, -1][index] * tileRoot.width * 0.28
                anchors.verticalCenterOffset: [-1, -1, 0, 1, 1, 1, 0, -1][index] * tileRoot.height * 0.28
            }
        }
    }

    // === SPINNER PROGRESS (Roman numerals I, II, III, IV, V) ===
    Row {
        anchors.centerIn: parent
        spacing: 1
        visible: spinnerSteps > 0 && revealed

        Repeater {
            model: spinnerSteps

            Rectangle {
                width: Math.max(12, (tileRoot.width - 8) / spinnerSteps - 1)
                height: width
                radius: 3
                color: {
                    if (index < spinnerProgress)
                        return "#44ff44"   // completed step — bright green
                    else if (index === spinnerProgress && spinnerProgress > 0)
                        return "#ffcc00"   // current step — yellow
                    else
                        return "#00000080" // pending step — dark
                }
                border.color: index < spinnerProgress ? "#00aa00" : "#ffffff60"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: ["I","II","III","IV","V"][index]
                    color: index < spinnerProgress ? "#003300" : "#ffffff"
                    font.pixelSize: parent.width * 0.55
                    font.bold: true
                }
            }
        }
    }

    // === SPINNER "CLICK TO ADVANCE" hint when pirate is here ===
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 2
        width: spinnerHint.width + 8
        height: spinnerHint.height + 4
        radius: 4
        color: "#dd000000"
        visible: spinnerSteps > 0 && revealed && spinnerProgress > 0 && spinnerProgress < spinnerSteps

        Text {
            id: spinnerHint
            anchors.centerIn: parent
            text: "Кликните!"
            color: "#ffcc00"
            font.pixelSize: tileRoot.width * 0.16
            font.bold: true
        }

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: parent.visible
            NumberAnimation { from: 0.4; to: 1.0; duration: 500 }
            NumberAnimation { from: 1.0; to: 0.4; duration: 500 }
        }
    }

    // === COIN COUNT OVERLAY ===
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        width: coinText.width + 6
        height: coinText.height + 4
        radius: 3
        color: "#dd000000"
        visible: coins > 0 || hasGalleon

        Text {
            id: coinText
            anchors.centerIn: parent
            text: coins > 0 ? ("$" + coins) : (hasGalleon ? "$3" : "")
            color: "#ffd700"
            font.pixelSize: tileRoot.width * 0.22
            font.bold: true
        }
    }

    // === TREASURE VALUE (Roman numeral) ===
    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 1
        width: treasureText.width + 4
        height: treasureText.height + 2
        radius: 3
        color: "#cc000000"
        visible: tileTypeName === "Treasure" && revealed && treasureValue > 0

        Text {
            id: treasureText
            anchors.centerIn: parent
            text: {
                var vals = ["", "I", "II", "III", "IV", "V"];
                return treasureValue <= 5 ? vals[treasureValue] : treasureValue.toString();
            }
            color: "#ffd700"
            font.pixelSize: tileRoot.width * 0.2
            font.bold: true
        }
    }

    // Click handler
    MouseArea {
        anchors.fill: parent
        onClicked: gameController.cellClicked(tileRow, tileCol)
        cursorShape: (isLand || isWater) ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
