import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    color: "#081018"

    signal openStm32Requested()
    signal openHistoryRequested()

    property bool connectionStatus: false
    property string warningMessage: "等待无人机数据"
    property string lastGpsTime: "--"
    property bool stm32PromptShown: false

    function updateWarnings() {
        var messages = []

        if (!connectionStatus) {
            messages.push("GPS 尚未连接")
        }
        if (sensorBackend.temperature < -20 || sensorBackend.temperature > 40) {
            messages.push("环境温度超限：" + sensorBackend.temperature.toFixed(1)
                          + " ℃（允许 -20~40 ℃）")
        }
        if (sensorBackend.humidity > 90) {
            messages.push("湿度异常：" + sensorBackend.humidity.toFixed(1) + " %")
        }
        if (Math.abs(backend.roll) > 50) {
            messages.push("横滚角超限：" + backend.roll.toFixed(1) + "°（允许 ±50°）")
        }
        if (Math.abs(backend.pitch) > 50) {
            messages.push("俯仰角超限：" + backend.pitch.toFixed(1) + "°（允许 ±50°）")
        }
        if (connectionStatus && backend.battery > 0 && backend.battery < 20) {
            messages.push("电量过低：" + backend.battery.toFixed(0) + " %")
        }
        if (backend.speed > 50) {
            messages.push("速度超限：" + backend.speed.toFixed(1) + " m/s（上限 50）")
        }
        if (backend.height < -100 || backend.height > 500) {
            messages.push("高度超限：" + backend.height.toFixed(1)
                          + " m（允许 -100~500 m）")
        }
        if (backend.accuracy > 50) {
            messages.push("GPS 精度异常：误差 " + backend.accuracy.toFixed(1)
                          + " m（上限 50 m）")
        }
        warningMessage = messages.length > 0
                ? messages.join("\n")
                : "系统运行正常"
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 14
        height: 48
        radius: 11
        color: "#101c26"
        border.color: "#263f4e"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 8
                color: "#146b7b"

                Text {
                    anchors.centerIn: parent
                    text: "▲"
                    color: "white"
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Text {
                text: "手机地面站"
                color: "#f4f7fa"
                font.pixelSize: 16
                font.bold: true
            }

            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: root.connectionStatus ? "#42d49b" : "#6d7780"
            }

            TextInput {
                Layout.preferredWidth: 210
                text: backend.serverRunning
                      ? backend.accessUrl : "手机服务 8080 不可用"
                readOnly: true
                selectByMouse: true
                color: backend.serverRunning ? "#58d7ed" : "#ff7e86"
                selectionColor: "#286273"
                font.pixelSize: 12
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Button {
                id: historyButton
                Layout.preferredWidth: 100
                Layout.preferredHeight: 34
                text: "历史数据"

                contentItem: Text {
                    text: historyButton.text
                    color: "#eaf4f8"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: historyButton.down ? "#24576d" : "#173848"
                    border.color: "#2d687e"
                }

                onClicked: root.openHistoryRequested()
            }

            Text {
                text: sensorBackend.telemetryStatus
                color: sensorBackend.telemetryConnected ? "#65dcae" : "#8397a3"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 260
            }

            Button {
                id: stm32Button
                Layout.preferredWidth: 120
                Layout.preferredHeight: 34
                text: sensorBackend.telemetryConnected
                      ? "STM32 控制 · 在线" : "STM32 控制"

                contentItem: Text {
                    text: stm32Button.text
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: sensorBackend.telemetryConnected
                           ? "#16815f" : "#17465c"
                    border.color: sensorBackend.telemetryConnected
                                  ? "#54e0ae" : "#2c6b84"
                }

                onClicked: root.openStm32Requested()
            }
        }
    }

    RowLayout {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14
        spacing: 14

        Rectangle {
            id: dataPanel
            Layout.preferredWidth: (root.width - 42) * 0.30
            Layout.minimumWidth: 340
            Layout.maximumWidth: 520
            Layout.fillHeight: true
            radius: 14
            color: "#101a24"
            border.color: "#263b4a"
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 280
                    Layout.maximumHeight: 280
                    Layout.preferredHeight: 280
                    radius: 11
                    color: "#111f2a"
                    border.color: "#243947"
                    clip: true

                    Dashboard {
                        anchors.fill: parent
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 54
                    Layout.maximumHeight: 54
                    Layout.preferredHeight: 54
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 9
                        color: "#132631"

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: sensorBackend.temperature.toFixed(1) + " ℃"
                                color: "#ffb56b"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "环境温度"
                                color: "#718a9b"
                                font.pixelSize: 10
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 9
                        color: "#132631"

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: sensorBackend.humidity.toFixed(1) + " %"
                                color: "#65c9ff"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "环境湿度"
                                color: "#718a9b"
                                font.pixelSize: 10
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 9
                        color: root.warningMessage === "系统运行正常"
                               ? "#14332c" : "#44282d"

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.connectionStatus ? root.lastGpsTime : "--"
                                color: root.connectionStatus ? "#73e2b8" : "#a0a9af"
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "GPS 更新"
                                color: "#718a9b"
                                font.pixelSize: 10
                            }
                        }
                    }
                }

                TabBar {
                    id: dataTabs
                    Layout.fillWidth: true
                    Layout.minimumHeight: 38
                    Layout.maximumHeight: 38
                    Layout.preferredHeight: 38
                    spacing: 4

                    background: Rectangle {
                        radius: 9
                        color: "#09131c"
                    }

                    Repeater {
                        model: ["数据曲线", "姿态仪", "告警"]

                        TabButton {
                            id: tabButton
                            required property string modelData
                            text: modelData
                            font.pixelSize: 12
                            font.bold: checked

                            contentItem: Text {
                                text: tabButton.text
                                color: tabButton.checked ? "#67d8ee" : "#718899"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font: tabButton.font
                            }

                            background: Rectangle {
                                radius: 7
                                color: tabButton.checked ? "#173848" : "transparent"
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: dataTabs.currentIndex

                    Rectangle {
                        color: "#111f2a"
                        radius: 11
                        clip: true

                        FlightCurve {
                            anchors.fill: parent
                        }
                    }

                    Rectangle {
                        color: "#111f2a"
                        radius: 11
                        clip: true

                        AttitudeIndicator {
                            anchors.fill: parent
                        }
                    }

                    Rectangle {
                        color: root.warningMessage === "系统运行正常"
                               ? "#102820" : "#321d22"
                        radius: 11
                        border.color: root.warningMessage === "系统运行正常"
                                      ? "#245d49" : "#71414a"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: root.warningMessage === "系统运行正常"
                                      ? "✓ 系统运行正常"
                                      : "! 需要注意"
                                color: root.warningMessage === "系统运行正常"
                                       ? "#70dfad" : "#ff8e96"
                                font.pixelSize: 17
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: "#ffffff18"
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: root.warningMessage
                                color: "#dce5eb"
                                font.pixelSize: 14
                                lineHeight: 1.45
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                }
            }
        }

        Rectangle {
            id: mapPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 560
            radius: 14
            color: "#101a24"
            border.color: "#263b4a"
            border.width: 1
            clip: true

            MapView {
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }

    Popup {
        id: stm32ConnectedPopup
        anchors.centerIn: parent
        width: 420
        height: 230
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 0

        background: Rectangle {
            radius: 14
            color: "#101d27"
            border.color: "#38b88c"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 12

            Text {
                text: "STM32 已连接"
                color: "#64e2b2"
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: "检测到 " + (sensorBackend.telemetryPeer || "STM32 设备")
                      + " 的遥测服务器已连接。是否进入 STM32 控制页面？"
                color: "#c8d6dd"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                Button {
                    text: "稍后"
                    onClicked: stm32ConnectedPopup.close()
                }

                Button {
                    text: "进入控制页面"
                    onClicked: {
                        stm32ConnectedPopup.close()
                        root.openStm32Requested()
                    }
                }
            }
        }
    }

    Connections {
        target: backend

        function onGpsChanged() {
            root.connectionStatus = true
            root.lastGpsTime = backend.gpsTime || "--"
            root.updateWarnings()
        }
    }

    Connections {
        target: sensorBackend

        function onSensorChanged() {
            root.updateWarnings()
        }

        function onTelemetryConnectionChanged() {
            if (sensorBackend.telemetryConnected
                    && !root.stm32PromptShown) {
                root.stm32PromptShown = true
                stm32ConnectedPopup.open()
            }
        }
    }

    Component.onCompleted: {
        updateWarnings()
        if (sensorBackend.telemetryConnected) {
            root.stm32PromptShown = true
            Qt.callLater(stm32ConnectedPopup.open)
        }
    }
}
