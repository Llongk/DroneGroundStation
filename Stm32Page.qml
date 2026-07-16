import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string warningText: "等待 STM32 遥测数据"

    signal backRequested

    // 根据固件告警、电量、温湿度、速度和返航距离生成告警列表。
    function updateWarnings() {
        var messages = [];
        if (!sensorBackend.telemetryConnected)
            messages.push("STM32 TCP 连接已断开");
        if (!sensorBackend.telemetryValid)
            messages.push("尚未收到有效遥测帧");
        if (sensorBackend.telemetryValid && sensorBackend.alarmCode !== 0)
            messages.push("飞控告警代码：" + sensorBackend.alarmCode);
        if (sensorBackend.telemetryValid && sensorBackend.stm32Battery < 20)
            messages.push("飞行器电量过低：" + sensorBackend.stm32Battery + " %");
        if (sensorBackend.telemetryValid && sensorBackend.mcuTemperature > 75)
            messages.push("MCU 温度过高：" + sensorBackend.mcuTemperature.toFixed(1) + " ℃");
        if (sensorBackend.telemetryValid && sensorBackend.mcuTemperature < -20)
            messages.push("MCU 温度过低：" + sensorBackend.mcuTemperature.toFixed(1) + " ℃");
        if (sensorBackend.telemetryValid && (sensorBackend.dhtTemperature < -20
                                             || sensorBackend.dhtTemperature > 50))
            messages.push("DHT 温度超限：" + sensorBackend.dhtTemperature.toFixed(1)
                          + " ℃（允许 -20~50 ℃）");
        if (sensorBackend.telemetryValid && (sensorBackend.shtTemperature < -20
                                             || sensorBackend.shtTemperature > 50))
            messages.push("SHT 温度超限：" + sensorBackend.shtTemperature.toFixed(1)
                          + " ℃（允许 -20~50 ℃）");
        if (sensorBackend.telemetryValid && (sensorBackend.dhtHumidity < 10
                                             || sensorBackend.dhtHumidity > 90))
            messages.push("DHT 湿度超限：" + sensorBackend.dhtHumidity.toFixed(1) + " %（允许 10~90%）");
        if (sensorBackend.telemetryValid && (sensorBackend.shtHumidity < 10
                                             || sensorBackend.shtHumidity > 90))
            messages.push("SHT 湿度超限：" + sensorBackend.shtHumidity.toFixed(1) + " %（允许 10~90%）");
        if (sensorBackend.telemetryValid && sensorBackend.stm32Speed > 50)
            messages.push("飞行速度超限：" + sensorBackend.stm32Speed.toFixed(1) + " m/s（上限 50）");
        if (sensorBackend.telemetryValid && (sensorBackend.targetAltitude < 0
                                             || sensorBackend.targetAltitude > 500))
            messages.push("目标高度超限：" + sensorBackend.targetAltitude.toFixed(1) + " m（允许 0~500 m）");
        if (sensorBackend.telemetryValid && sensorBackend.rthDistance > 2000)
            messages.push("返航距离超限：" + sensorBackend.rthDistance.toFixed(1) + " m（上限 2000 m）");
        if (sensorBackend.telemetryValid && Math.abs(sensorBackend.stm32Latitude) < 0.000001
                && Math.abs(sensorBackend.stm32Longitude) < 0.000001)
            messages.push("STM32 定位坐标无效 (0, 0)");

        warningText = messages.length > 0 ? messages.join("\n") : "STM32 系统运行正常";
    }

    color: "#081018"

    Component.onCompleted: updateWarnings()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            border.color: "#243b49"
            color: "#101c26"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 16
                spacing: 12

                Button {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 84
                    text: "← 手机主页"

                    onClicked: root.backRequested()
                }
                Button {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 84
                    text: "重新连接"

                    onClicked: sensorBackend.reconnectTelemetry()
                }
                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 34
                    color: "#d37c22"
                    radius: 9

                    Text {
                        anchors.centerIn: parent
                        color: "white"
                        font.bold: true
                        font.pixelSize: 16
                        text: "▲"
                    }
                }
                Column {
                    spacing: 1

                    Text {
                        color: "#f3f7f9"
                        font.bold: true
                        font.pixelSize: 18
                        text: "STM32 无人机控制中心"
                    }
                    Text {
                        color: "#8098a6"
                        font.pixelSize: 10
                        text: sensorBackend.telemetryValid ? sensorBackend.deviceId + " · 协议 v"
                                                             + sensorBackend.protocolVersion :
                                                             "等待遥测数据"
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: connectionRow.implicitWidth + 22
                    border.color: sensorBackend.telemetryConnected ? "#2d8c6a" : "#75444d"
                    color: sensorBackend.telemetryConnected ? "#173c31" : "#3a282d"
                    radius: 16

                    Row {
                        id: connectionRow

                        anchors.centerIn: parent
                        spacing: 7

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            color: sensorBackend.telemetryConnected ? "#52e0aa" : "#ef7882"
                            height: 9
                            radius: 5
                            width: 9
                        }
                        Text {
                            color: "#e1ecef"
                            font.bold: true
                            font.pixelSize: 11
                            text: sensorBackend.telemetryConnected ? "已连接 "
                                                                     + sensorBackend.telemetryPeer :
                                                                     sensorBackend.telemetryStatus
                        }
                    }
                }
                Text {
                    color: "#7ed8e7"
                    font.pixelSize: 12
                    text: sensorBackend.telemetryValid ? "序号 #" + sensorBackend.sequence + "  ·  "
                                                         + sensorBackend.telemetryUpdateTime : "--"
                }
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 14

            Rectangle {
                Layout.fillHeight: true
                Layout.maximumWidth: 570
                Layout.preferredWidth: Math.max(390, root.width * 0.34)
                border.color: "#263b4a"
                color: "#101a24"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.maximumHeight: 70
                        Layout.minimumHeight: 70
                        Layout.preferredHeight: 70
                        spacing: 8

                        Rectangle {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            color: "#132631"
                            radius: 9

                            Column {
                                anchors.centerIn: parent
                                spacing: 3

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#52d5ea"
                                    font.bold: true
                                    font.pixelSize: 15
                                    text: sensorBackend.telemetryValid
                                          ? sensorBackend.targetAltitude.toFixed(1) + " m" : "--"
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#78909e"
                                    font.pixelSize: 10
                                    text: "目标高度"
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
                                spacing: 3

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#66dfa9"
                                    font.bold: true
                                    font.pixelSize: 15
                                    text: sensorBackend.telemetryValid
                                          ? sensorBackend.stm32Speed.toFixed(1) + " m/s" : "--"
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#78909e"
                                    font.pixelSize: 10
                                    text: "实时速度"
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
                                spacing: 3

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#f1c55b"
                                    font.bold: true
                                    font.pixelSize: 15
                                    text: sensorBackend.telemetryValid ? sensorBackend.stm32Battery
                                                                         + " %" : "--"
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: "#78909e"
                                    font.pixelSize: 10
                                    text: "飞行器电量"
                                }
                            }
                        }
                    }
                    TabBar {
                        id: stmTabs

                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        spacing: 4

                        background: Rectangle {
                            color: "#09131c"
                            radius: 8
                        }

                        TabButton {
                            text: "遥测参数"
                        }
                        TabButton {
                            text: "实时曲线"
                        }
                        TabButton {
                            text: "告警"
                        }
                    }
                    StackLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        currentIndex: stmTabs.currentIndex

                        TelemetryPanel {
                        }
                        Stm32FlightCurve {
                        }
                        Rectangle {
                            border.color: root.warningText === "STM32 系统运行正常" ? "#2b7359" :
                                                                                "#78434c"
                            color: root.warningText === "STM32 系统运行正常" ? "#102a22" : "#342126"
                            radius: 10

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Text {
                                    color: root.warningText === "STM32 系统运行正常" ? "#6ce1ad" :
                                                                                 "#ff8c96"
                                    font.bold: true
                                    font.pixelSize: 18
                                    text: root.warningText === "STM32 系统运行正常" ? "✓ STM32 系统运行正常" :
                                                                                "! STM32 状态提醒"
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: "#ffffff18"
                                }
                                Text {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    color: "#d7e3e8"
                                    font.pixelSize: 14
                                    lineHeight: 1.5
                                    text: root.warningText
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
            Stm32MapView {
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
        }
        Rectangle {
            id: flightControlPanel

            Layout.bottomMargin: 14
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.preferredHeight: 138
            Layout.rightMargin: 14
            border.color: sensorBackend.manualControl ? "#b9782b" : "#27765f"
            border.width: 1
            color: "#101c26"
            radius: 12

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 11
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    spacing: 10

                    Text {
                        color: "#f2f6f8"
                        font.bold: true
                        font.pixelSize: 15
                        text: "飞行控制"
                    }
                    Rectangle {
                        Layout.preferredHeight: 23
                        Layout.preferredWidth: modeText.implicitWidth + 18
                        border.color: sensorBackend.manualControl ? "#c98a3d" : "#2c8c69"
                        color: sensorBackend.manualControl ? "#573b20" : "#173c31"
                        radius: 12

                        Text {
                            id: modeText

                            anchors.centerIn: parent
                            color: sensorBackend.manualControl ? "#ffc66f" : "#63e2ae"
                            font.bold: true
                            font.pixelSize: 11
                            text: sensorBackend.manualControl ? "人工接管" : "自主飞行"
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: sensorBackend.telemetryConnected ? "#87a8b8" : "#ef858d"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        text: sensorBackend.commandStatus
                    }
                    Text {
                        color: "#617c8c"
                        font.pixelSize: 10
                        text: "命令 JSON / TCP " + sensorBackend.telemetryTarget
                    }
                }
                RowLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 7

                    Button {
                        id: freeFlightButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 112
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "自由飞行\n预设航线"

                        background: Rectangle {
                            border.color: "#3baa7e"
                            color: freeFlightButton.down ? "#207354" : "#18553f"
                            radius: 8
                        }
                        contentItem: Text {
                            color: freeFlightButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: freeFlightButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("free_flight", headingInput.value,
                                                                   altitudeInput.value,
                                                                   speedInput.value)
                    }
                    Button {
                        id: takeoffButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 68
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "起飞"

                        background: Rectangle {
                            border.color: "#3b9a78"
                            color: takeoffButton.down ? "#278967" : "#1c694e"
                            radius: 8
                        }
                        contentItem: Text {
                            color: takeoffButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: takeoffButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("takeoff", headingInput.value,
                                                                   altitudeInput.value, Math.max(5,
                                                                                                 speedInput.value))
                    }
                    Button {
                        id: hoverButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 68
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "悬停"

                        background: Rectangle {
                            border.color: "#4387a4"
                            color: hoverButton.down ? "#32728d" : "#22536a"
                            radius: 8
                        }
                        contentItem: Text {
                            color: hoverButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: hoverButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("hover", headingInput.value,
                                                                   altitudeInput.value, 0)
                    }
                    Button {
                        id: loiterButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 68
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "盘旋"

                        background: Rectangle {
                            border.color: "#477baa"
                            color: loiterButton.down ? "#376b9a" : "#284f75"
                            radius: 8
                        }
                        contentItem: Text {
                            color: loiterButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: loiterButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("loiter", headingInput.value,
                                                                   altitudeInput.value, Math.max(10,
                                                                                                 speedInput.value))
                    }
                    Button {
                        id: headingButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 88
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "改变航向"

                        background: Rectangle {
                            border.color: "#a57b3a"
                            color: headingButton.down ? "#956b2c" : "#755320"
                            radius: 8
                        }
                        contentItem: Text {
                            color: headingButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: headingButton.text
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                        }

                        onClicked: sensorBackend.sendFlightCommand("set_heading", headingInput.value,
                                                                   altitudeInput.value, Math.max(5,
                                                                                                 speedInput.value))
                    }
                    Button {
                        id: rthButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 68
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "返航"

                        background: Rectangle {
                            border.color: "#ad8240"
                            color: rthButton.down ? "#9b7131" : "#7b5523"
                            radius: 8
                        }
                        contentItem: Text {
                            color: rthButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: rthButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("rth", headingInput.value,
                                                                   altitudeInput.value,
                                                                   speedInput.value)
                    }
                    Button {
                        id: landButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 68
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "降落"

                        background: Rectangle {
                            border.color: "#b4535b"
                            color: landButton.down ? "#a0444c" : "#7a3035"
                            radius: 8
                        }
                        contentItem: Text {
                            color: landButton.enabled ? "white" : "#71818a"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            text: landButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sensorBackend.sendFlightCommand("land", headingInput.value,
                                                                   altitudeInput.value, 0)
                    }
                    Rectangle {
                        Layout.bottomMargin: 4
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.topMargin: 4
                        color: "#304754"
                    }
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 82
                        spacing: 2

                        Text {
                            color: "#8da2ae"
                            font.pixelSize: 10
                            text: "航向角 °"
                        }
                        SpinBox {
                            id: headingInput

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            editable: true
                            from: 0
                            to: 359
                            value: sensorBackend.telemetryValid ? Math.round(sensorBackend.heading) :
                                                                  0
                        }
                    }
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 86
                        spacing: 2

                        Text {
                            color: "#8da2ae"
                            font.pixelSize: 10
                            text: "目标高度 m"
                        }
                        SpinBox {
                            id: altitudeInput

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            editable: true
                            from: 0
                            to: 500
                            value: sensorBackend.telemetryValid ? Math.round(
                                                                      sensorBackend.targetAltitude) :
                                                                  50
                        }
                    }
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 82
                        spacing: 2

                        Text {
                            color: "#8da2ae"
                            font.pixelSize: 10
                            text: "速度 m/s"
                        }
                        SpinBox {
                            id: speedInput

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            editable: true
                            from: 0
                            to: 50
                            value: sensorBackend.telemetryValid ? Math.round(
                                                                      sensorBackend.stm32Speed) : 5
                        }
                    }
                }
            }
        }
    }
    Connections {
        // 遥测更新后重新计算 STM32 告警。
        function onTelemetryChanged() {
            root.updateWarnings();
        }
        // 连接状态变化后刷新 STM32 告警和状态显示。
        function onTelemetryConnectionChanged() {
            root.updateWarnings();
        }

        target: sensorBackend
    }
}
