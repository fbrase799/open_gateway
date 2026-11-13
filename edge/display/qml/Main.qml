import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 800
    height: 480
    title: "Open Gateway"
    color: "#14181f"

    header: ToolBar {
        Label {
            text: "Open Gateway  ·  " + gateway.link
            color: "#e2a354"
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: 16
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 12

        ListView {
            id: deviceList
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: sourcePanel.left
            anchors.rightMargin: 12
            model: gateway.devices
            spacing: 8
            clip: true
            delegate: Rectangle {
                required property var modelData
                width: deviceList.width
                height: 64
                radius: 8
                color: "#1d232c"
                Text {
                    text: modelData.name
                    color: "#e7e1d6"
                    font.pixelSize: 22
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.62
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.status
                    color: modelData.status === "connected" ? "#8fd0a8" : "#e2a354"
                    font.pixelSize: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Column {
            id: sourcePanel
            width: 280
            spacing: 8
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Text { text: "Sources"; color: "#e7e1d6"; font.pixelSize: 20 }
            Repeater {
                model: gateway.sources
                delegate: Button {
                    required property var modelData
                    width: 280
                    height: 56
                    text: (modelData.state === "active" ? "Disable " : "Enable ") + modelData.name
                    onClicked: gateway.setSourceActive(modelData.id, modelData.state !== "active")
                }
            }
        }
    }
}
