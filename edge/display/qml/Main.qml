import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    visibility: Window.FullScreen
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
        anchors.margins: 16

        ListView {
            id: dashboard
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: sensorPanel.left
            anchors.rightMargin: 16
            model: gateway.dashboard
            spacing: 12
            clip: true
            delegate: Rectangle {
                required property var modelData
                width: dashboard.width
                height: modelData.commandId ? 168 : 96
                radius: 10
                color: "#1d232c"
                Text {
                    id: title
                    text: modelData.name
                    color: "#e7e1d6"
                    font.pixelSize: 28
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 20
                    anchors.topMargin: 16
                    width: parent.width * 0.7
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.status
                    color: modelData.status === "connected" ? "#8fd0a8" : "#e2a354"
                    font.pixelSize: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: title.verticalCenter
                }
                Text {
                    text: modelData.values.length ? modelData.values : "No readings yet"
                    color: "#c8c2b8"
                    font.pixelSize: 20
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: title.bottom
                    anchors.margins: 20
                    elide: Text.ElideRight
                }
                Row {
                    visible: modelData.commandId
                    spacing: 12
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                    Slider {
                        id: duty
                        width: parent.width - apply.width - 12
                        from: 0
                        to: 100
                        value: 40
                        stepSize: 1
                    }
                    Button {
                        id: apply
                        text: modelData.commandName
                        onClicked: gateway.sendCommand(modelData.id, modelData.commandId, duty.value)
                    }
                }
            }
        }

        Text {
            visible: gateway.dashboard.length === 0
            text: "Enable a sensor to see it here"
            color: "#8b867e"
            font.pixelSize: 22
            anchors.centerIn: dashboard
        }

        Column {
            id: sensorPanel
            width: 360
            spacing: 8
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            Text { text: "Sensors"; color: "#e7e1d6"; font.pixelSize: 22 }
            Repeater {
                model: gateway.sensors
                delegate: Rectangle {
                    required property var modelData
                    width: 360
                    height: 72
                    radius: 8
                    color: "#1d232c"
                    opacity: modelData.availability === "greyed" ? 0.4 : 1
                    Text {
                        text: modelData.name
                        color: "#e7e1d6"
                        font.pixelSize: 16
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: 210
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: modelData.availability === "greyed"
                        text: "Not connected"
                        color: "#8b867e"
                        font.pixelSize: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Button {
                        visible: modelData.canToggle
                        enabled: modelData.canToggle
                        width: 108
                        height: 44
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.availability === "enabled" ? "Disable" : "Enable"
                        onClicked: gateway.setSourceActive(modelData.id, modelData.availability !== "enabled")
                    }
                }
            }
        }
    }
}
