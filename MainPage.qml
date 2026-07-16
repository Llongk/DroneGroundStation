import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property bool connectionStatus: false
    property string lastGpsTime: "--"
    property bool stm32PromptShown: false
    property string warningMessage: "等待无人机数据"

    signal openHistoryRequested
    signal openStm32Requested

    // 根据手机传感器、飞行数据和连接状态生成实时告警。
    function updateWarnings() {
        var messages = [];

        if (!connectionStatus) {
            messages.push("GPS 尚未连接");
        }
        if (sensorBackend.temperature < -20 || sensorBackend.temperature > 40) {
            messages.push("环境温度超限：" + sensorBackend.temperature.toFixed(1) + " ℃（允许 -20~40 ℃）");
        }
        if (sensorBackend.humidity > 90) {
            messages.push("湿度异常：" + sensorBackend.humidity.toFixed(1) + " %");
        }
        if (Math.abs(backend.roll) > 50) {
            messages.push("横滚角超限：" + backend.roll.toFixed(1) + "°（允许 ±50°）");
        }
        if (Math.abs(backend.pitch) > 50) {
            messages.push("俯仰角超限：" + backend.pitch.toFixed(1) + "°（允许 ±50°）");
        }
        if (connectionStatus && backend.battery > 0 && backend.battery < 20) {
            messages.push("电量过低：" + backend.battery.toFixed(0) + " %");
        }
        if (backend.speed > 50) {
            messages.push("速度超限：" + backend.speed.toFixed(1) + " m/s（上限 50）");
        }
        if (backend.height < -100 || backend.height > 500) {
            messages.push("高度超限：" + backend.height.toFixed(1) + " m（允许 -100~500 m）");
        }
        if (backend.accuracy > 50) {
            messages.push("GPS 精度异常：误差 " + backend.accuracy.toFixed(1) + " m（上限 50 m）");
        }
        warningMessage = messages.length > 0 ? messages.join("\n") : "系统运行正常";
    }

    color: "#081018"

    Component.onCompleted: {
        updateWarnings();
        if (sensorBackend.telemetryConnected) {
            root.stm32PromptShown = true;
            Qt.callLater(stm32ConnectedPopup.open);
        }
    }

    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.margins: 14
        anchors.right: parent.right
        anchors.top: parent.top
        border.color: "#263f4e"
        color: "#101c26"
        height: 48
        radius: 11

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 10

            Rectangle {
                Layout.preferredHeight: 30
                Layout.preferredWidth: 30
                color: "#146b7b"
                radius: 8

                Text {
                    anchors.centerIn: parent
                    color: "white"
                    font.bold: true
                    font.pixelSize: 15
                    text: "▲"
                }
            }
            Text {
                color: "#f4f7fa"
                font.bold: true
                font.pixelSize: 16
                text: "手机地面站"
            }
            Rectangle {
                Layout.preferredHeight: 8
                Layout.preferredWidth: 8
                color: root.connectionStatus ? "#42d49b" : "#6d7780"
                radius: 4
            }
            TextInput {
                Layout.preferredWidth: 210
                color: backend.serverRunning ? "#58d7ed" : "#ff7e86"
                font.bold: true
                font.pixelSize: 12
                readOnly: true
                selectByMouse: true
                selectionColor: "#286273"
                text: backend.serverRunning ? backend.accessUrl : "手机服务 8080 不可用"
                verticalAlignment: Text.AlignVCenter
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                id: historyButton

                Layout.preferredHeight: 34
                Layout.preferredWidth: 100
                text: "历史数据"

                background: Rectangle {
                    border.color: "#2d687e"
                    color: historyButton.down ? "#24576d" : "#173848"
                    radius: 8
                }
                contentItem: Text {
                    color: "#eaf4f8"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    text: historyButton.text
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.openHistoryRequested()
            }
            Text {
                Layout.maximumWidth: 260
                color: sensorBackend.telemetryConnected ? "#65dcae" : "#8397a3"
                elide: Text.ElideRight
                font.pixelSize: 11
                text: sensorBackend.telemetryStatus
            }
            Button {
                id: stm32Button

                Layout.preferredHeight: 34
                Layout.preferredWidth: 120
                text: sensorBackend.telemetryConnected ? "STM32 控制 · 在线" : "STM32 控制"

                background: Rectangle {
                    border.color: sensorBackend.telemetryConnected ? "#54e0ae" : "#2c6b84"
                    color: sensorBackend.telemetryConnected ? "#16815f" : "#17465c"
                    radius: 8
                }
                contentItem: Text {
                    color: "white"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    text: stm32Button.text
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.openStm32Requested()
            }
        }
    }
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.top: topBar.bottom
        anchors.topMargin: 12
        spacing: 14

        Rectangle {
            id: dataPanel

            Layout.fillHeight: true
            Layout.maximumWidth: 520
            Layout.minimumWidth: 340
            Layout.preferredWidth: (root.width - 42) * 0.30
            border.color: "#263b4a"
            border.width: 1
            clip: true
            color: "#101a24"
            radius: 14

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.maximumHeight: 280
                    Layout.minimumHeight: 280
                    Layout.preferredHeight: 280
                    border.color: "#243947"
                    clip: true
                    color: "#111f2a"
                    radius: 11

                    Dashboard {
                        anchors.fill: parent
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumHeight: 54
                    Layout.minimumHeight: 54
                    Layout.preferredHeight: 54
                    spacing: 8

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: "#132631"
                        radius: 9

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#ffb56b"
                                font.bold: true
                                font.pixelSize: 16
                                text: sensorBackend.temperature.toFixed(1) + " ℃"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#718a9b"
                                font.pixelSize: 10
                                text: "环境温度"
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: "#132631"
                        radius: 9

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#65c9ff"
                                font.bold: true
                                font.pixelSize: 16
                                text: sensorBackend.humidity.toFixed(1) + " %"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#718a9b"
                                font.pixelSize: 10
                                text: "环境湿度"
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: root.warningMessage === "系统运行正常" ? "#14332c" : "#44282d"
                        radius: 9

                        Column {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: root.connectionStatus ? "#73e2b8" : "#a0a9af"
                                font.bold: true
                                font.pixelSize: 15
                                text: root.connectionStatus ? root.lastGpsTime : "--"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "#718a9b"
                                font.pixelSize: 10
                                text: "GPS 更新"
                            }
                        }
                    }
                }
                TabBar {
                    id: dataTabs

                    Layout.fillWidth: true
                    Layout.maximumHeight: 38
                    Layout.minimumHeight: 38
                    Layout.preferredHeight: 38
                    spacing: 4

                    background: Rectangle {
                        color: "#09131c"
                        radius: 9
                    }

                    Repeater {
                        model: ["数据曲线", "姿态仪", "告警"]

                        TabButton {
                            id: tabButton

                            required property string modelData

                            font.bold: checked
                            font.pixelSize: 12
                            text: modelData

                            background: Rectangle {
                                color: tabButton.checked ? "#173848" : "transparent"
                                radius: 7
                            }
                            contentItem: Text {
                                color: tabButton.checked ? "#67d8ee" : "#718899"
                                font: tabButton.font
                                horizontalAlignment: Text.AlignHCenter
                                text: tabButton.text
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
                StackLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    currentIndex: dataTabs.currentIndex

                    Rectangle {
                        clip: true
                        color: "#111f2a"
                        radius: 11

                        FlightCurve {
                            anchors.fill: parent
                        }
                    }
                    Rectangle {
                        clip: true
                        color: "#111f2a"
                        radius: 11

                        AttitudeIndicator {
                            anchors.fill: parent
                        }
                    }
                    Rectangle {
                        border.color: root.warningMessage === "系统运行正常" ? "#245d49" : "#71414a"
                        color: root.warningMessage === "系统运行正常" ? "#102820" : "#321d22"
                        radius: 11

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                color: root.warningMessage === "系统运行正常" ? "#70dfad" : "#ff8e96"
                                font.bold: true
                                font.pixelSize: 17
                                text: root.warningMessage === "系统运行正常" ? "✓ 系统运行正常" : "! 需要注意"
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: "#ffffff18"
                            }
                            Text {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                color: "#dce5eb"
                                font.pixelSize: 14
                                lineHeight: 1.45
                                text: root.warningMessage
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            id: mapPanel

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 560
            border.color: "#263b4a"
            border.width: 1
            clip: true
            color: "#101a24"
            radius: 14

            MapView {
                anchors.fill: parent
                anchors.margins: 1
            }
        }
    }
    Popup {
        id: stm32ConnectedPopup

        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape
        focus: true
        height: 230
        modal: true
        padding: 0
        width: 420

        background: Rectangle {
            border.color: "#38b88c"
            border.width: 1
            color: "#101d27"
            radius: 14
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 12

            Text {
                color: "#64e2b2"
                font.bold: true
                font.pixelSize: 22
                text: "STM32 已连接"
            }
            Text {
                Layout.fillWidth: true
                color: "#c8d6dd"
                font.pixelSize: 14
                text: "检测到 " + (sensorBackend.telemetryPeer || "STM32 设备")
                      + " 的遥测服务器已连接。是否进入 STM32 控制页面？"
                wrapMode: Text.WordWrap
            }
            Item {
                Layout.fillHeight: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "稍后"

                    onClicked: stm32ConnectedPopup.close()
                }
                Button {
                    text: "进入控制页面"

                    onClicked: {
                        stm32ConnectedPopup.close();
                        root.openStm32Requested();
                    }
                }
            }
        }
    }
    Connections {
        // 手机 GPS 更新后刷新连接状态和告警。
        function onGpsChanged() {
            root.connectionStatus = true;
            root.lastGpsTime = backend.gpsTime || "--";
            root.updateWarnings();
        }

        target: backend
    }
    Connections {
        // 手机温湿度更新后重新计算告警。
        function onSensorChanged() {
            root.updateWarnings();
        }
        // STM32 连接状态变化时更新入口提示和告警。
        function onTelemetryConnectionChanged() {
            if (sensorBackend.telemetryConnected && !root.stm32PromptShown) {
                root.stm32PromptShown = true;
                stm32ConnectedPopup.open();
            }
        }

        target: sensorBackend
    }
}
