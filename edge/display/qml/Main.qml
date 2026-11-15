import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    visibility: Window.FullScreen
    title: appTitle
    color: "#DDF3FE"

    header: Rectangle {
        color: "#000233"
        implicitHeight: 64

        Text {
            text: appTitle
            color: "#C5D4FF"
            font.pixelSize: 20
            font.weight: Font.DemiBold
            anchors.left: parent.left
            anchors.right: linkPill.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 28
            anchors.rightMargin: 16
            elide: Text.ElideRight
        }

        Rectangle {
            id: linkPill
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.verticalCenter: parent.verticalCenter
            width: linkLabel.implicitWidth + 28
            height: 34
            radius: 8
            color: "#0fffffff"
            border.color: "#1fffffff"
            border.width: 1

            Text {
                id: linkLabel
                anchors.centerIn: parent
                text: gateway.link
                color: gateway.link === "live" ? "#27ae60" : "#ffffffb8"
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 24

        ListView {
            id: dashboard
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: sensorPanel.left
            anchors.rightMargin: 20
            model: gateway.dashboard
            spacing: 16
            clip: true
            delegate: Rectangle {
                required property var modelData
                width: dashboard.width
                height: modelData.commandId ? 176 : 108
                radius: 16
                color: "#FFFFFF"
                border.color: "#1a002450"
                border.width: 1

                Text {
                    id: cardTitle
                    text: modelData.name
                    color: "#0F172A"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 24
                    anchors.topMargin: 18
                    width: parent.width * 0.62
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.status
                    color: modelData.status === "connected" ? "#27ae60" : "#475569"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: cardTitle.verticalCenter
                }
                Text {
                    text: modelData.values.length ? modelData.values : "No readings yet"
                    color: "#475569"
                    font.pixelSize: 16
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: cardTitle.bottom
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.topMargin: 10
                    elide: Text.ElideRight
                }
                Row {
                    visible: modelData.commandId
                    spacing: 12
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.bottomMargin: 16
                    Slider {
                        id: duty
                        width: parent.width - apply.width - 12
                        from: 0
                        to: 100
                        value: 40
                        stepSize: 1
                        background: Rectangle {
                            x: duty.leftPadding
                            y: duty.topPadding + duty.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 4
                            width: duty.availableWidth
                            height: 4
                            radius: 2
                            color: "#A7E2FF"
                            Rectangle {
                                width: duty.visualPosition * parent.width
                                height: parent.height
                                color: "#0049B0"
                                radius: 2
                            }
                        }
                        handle: Rectangle {
                            x: duty.leftPadding + duty.visualPosition * (duty.availableWidth - width)
                            y: duty.topPadding + duty.availableHeight / 2 - height / 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#0049B0"
                            border.color: "#FFFFFF"
                            border.width: 2
                        }
                    }
                    Button {
                        id: apply
                        width: 120
                        height: 40
                        text: modelData.commandName
                        onClicked: gateway.sendCommand(modelData.id, modelData.commandId, duty.value)
                        background: Rectangle {
                            radius: 8
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#0049B0" }
                                GradientStop { position: 1.0; color: "#7c3aed" }
                            }
                        }
                        contentItem: Text {
                            text: apply.text
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Text {
            visible: gateway.dashboard.length === 0
            text: "Enable a sensor to see it here"
            color: "#475569"
            font.pixelSize: 18
            anchors.centerIn: dashboard
        }

        Rectangle {
            id: sensorPanel
            width: 360
            radius: 16
            color: "#FFFFFF"
            border.color: "#1a002450"
            border.width: 1
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            Text {
                id: sensorsTitle
                text: "Sensors"
                color: "#0F172A"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 20
                anchors.topMargin: 20
            }

            Column {
                anchors.top: sensorsTitle.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                anchors.topMargin: 14
                spacing: 10

                Repeater {
                    model: gateway.sensors
                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool enabledRow: modelData.availability === "enabled"
                        width: 328
                        height: 76
                        radius: 10
                        color: enabledRow ? "#73A7E2FF" : "#DDF3FE"
                        border.width: 2
                        border.color: enabledRow ? "#0049B0" : "transparent"
                        opacity: modelData.availability === "greyed" ? 0.5 : 1

                        Text {
                            text: modelData.name
                            color: "#0F172A"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: modelData.canToggle ? 180 : 200
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                        }
                        Text {
                            visible: modelData.availability === "greyed"
                            text: "Not connected"
                            color: "#475569"
                            font.pixelSize: 13
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Button {
                            visible: modelData.canToggle
                            enabled: modelData.canToggle
                            width: 108
                            height: 40
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: enabledRow ? "Disable" : "Enable"
                            onClicked: gateway.setSourceActive(modelData.id, !enabledRow)
                            background: Rectangle {
                                radius: 8
                                color: enabledRow ? "#FFFFFF" : "transparent"
                                border.width: enabledRow ? 1 : 0
                                border.color: "#38002450"
                                gradient: enabledRow ? null : primaryGradient
                            }
                            contentItem: Text {
                                text: parent.text
                                color: enabledRow ? "#0049B0" : "#FFFFFF"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Gradient {
                                id: primaryGradient
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#0049B0" }
                                GradientStop { position: 1.0; color: "#7c3aed" }
                            }
                        }
                    }
                }
            }
        }
    }
}
