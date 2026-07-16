import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#081018"

    signal backRequested()
    property string warningText: "等待 STM32 遥测数据"

    function updateWarnings() {
        var messages = []
        if (!sensorBackend.telemetryConnected)
            messages.push("STM32 TCP 连接已断开")
        if (!sensorBackend.telemetryValid)
            messages.push("尚未收到有效遥测帧")
        if (sensorBackend.telemetryValid && sensorBackend.alarmCode !== 0)
            messages.push("飞控告警代码：" + sensorBackend.alarmCode)
        if (sensorBackend.telemetryValid && sensorBackend.stm32Battery < 20)
            messages.push("飞行器电量过低：" + sensorBackend.stm32Battery + " %")
        if (sensorBackend.telemetryValid && sensorBackend.mcuTemperature > 75)
            messages.push("MCU 温度过高："
                          + sensorBackend.mcuTemperature.toFixed(1) + " ℃")
        if (sensorBackend.telemetryValid && sensorBackend.mcuTemperature < -20)
            messages.push("MCU 温度过低："
                          + sensorBackend.mcuTemperature.toFixed(1) + " ℃")
        if (sensorBackend.telemetryValid
                && (sensorBackend.dhtTemperature < -20
                    || sensorBackend.dhtTemperature > 50))
            messages.push("DHT 温度超限：" + sensorBackend.dhtTemperature.toFixed(1)
                          + " ℃（允许 -20~50 ℃）")
        if (sensorBackend.telemetryValid
                && (sensorBackend.shtTemperature < -20
                    || sensorBackend.shtTemperature > 50))
            messages.push("SHT 温度超限：" + sensorBackend.shtTemperature.toFixed(1)
                          + " ℃（允许 -20~50 ℃）")
        if (sensorBackend.telemetryValid
                && (sensorBackend.dhtHumidity < 10
                    || sensorBackend.dhtHumidity > 90))
            messages.push("DHT 湿度超限：" + sensorBackend.dhtHumidity.toFixed(1)
                          + " %（允许 10~90%）")
        if (sensorBackend.telemetryValid
                && (sensorBackend.shtHumidity < 10
                    || sensorBackend.shtHumidity > 90))
            messages.push("SHT 湿度超限：" + sensorBackend.shtHumidity.toFixed(1)
                          + " %（允许 10~90%）")
        if (sensorBackend.telemetryValid && sensorBackend.stm32Speed > 50)
            messages.push("飞行速度超限：" + sensorBackend.stm32Speed.toFixed(1)
                          + " m/s（上限 50）")
        if (sensorBackend.telemetryValid
                && (sensorBackend.targetAltitude < 0
                    || sensorBackend.targetAltitude > 500))
            messages.push("目标高度超限：" + sensorBackend.targetAltitude.toFixed(1)
                          + " m（允许 0~500 m）")
        if (sensorBackend.telemetryValid && sensorBackend.rthDistance > 2000)
            messages.push("返航距离超限：" + sensorBackend.rthDistance.toFixed(1)
                          + " m（上限 2000 m）")
        if (sensorBackend.telemetryValid
                && Math.abs(sensorBackend.stm32Latitude) < 0.000001
                && Math.abs(sensorBackend.stm32Longitude) < 0.000001)
            messages.push("STM32 定位坐标无效 (0, 0)")

        warningText = messages.length > 0
                ? messages.join("\n") : "STM32 系统运行正常"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#101c26"
            border.color: "#243b49"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 16
                spacing: 12

                Button {
                    Layout.preferredWidth: 84
                    Layout.preferredHeight: 34
                    text: "← 手机主页"
                    onClicked: root.backRequested()
                }

                Button {
                    Layout.preferredWidth: 84
                    Layout.preferredHeight: 34
                    text: "重新连接"
                    onClicked: sensorBackend.reconnectTelemetry()
                }

                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 9
                    color: "#d37c22"

                    Text {
                        anchors.centerIn: parent
                        text: "▲"
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Column {
                    spacing: 1

                    Text {
                        text: "STM32 无人机控制中心"
                        color: "#f3f7f9"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Text {
                        text: sensorBackend.telemetryValid
                              ? sensorBackend.deviceId + " · 协议 v"
                                + sensorBackend.protocolVersion
                              : "等待遥测数据"
                        color: "#8098a6"
                        font.pixelSize: 10
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: connectionRow.implicitWidth + 22
                    Layout.preferredHeight: 32
                    radius: 16
                    color: sensorBackend.telemetryConnected
                           ? "#173c31" : "#3a282d"
                    border.color: sensorBackend.telemetryConnected
                                  ? "#2d8c6a" : "#75444d"

                    Row {
                        id: connectionRow
                        anchors.centerIn: parent
                        spacing: 7

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 9
                            height: 9
                            radius: 5
                            color: sensorBackend.telemetryConnected
                                   ? "#52e0aa" : "#ef7882"
                        }

                        Text {
                        text: sensorBackend.telemetryConnected
                                  ? "已连接 " + sensorBackend.telemetryPeer
                                  : sensorBackend.telemetryStatus
                            color: "#e1ecef"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }

                Text {
                    text: sensorBackend.telemetryValid
                          ? "序号 #" + sensorBackend.sequence
                            + "  ·  " + sensorBackend.telemetryUpdateTime
                          : "--"
                    color: "#7ed8e7"
                    font.pixelSize: 12
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 14
            spacing: 14

            Rectangle {
                Layout.preferredWidth: Math.max(390, root.width * 0.34)
                Layout.maximumWidth: 570
                Layout.fillHeight: true
                radius: 12
                color: "#101a24"
                border.color: "#263b4a"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 70
                        Layout.maximumHeight: 70
                        Layout.preferredHeight: 70
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            radius: 9; color: "#132631"
                            Column { anchors.centerIn: parent; spacing: 3
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: sensorBackend.telemetryValid ? sensorBackend.targetAltitude.toFixed(1) + " m" : "--"; color: "#52d5ea"; font.pixelSize: 15; font.bold: true }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "目标高度"; color: "#78909e"; font.pixelSize: 10 }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            radius: 9; color: "#132631"
                            Column { anchors.centerIn: parent; spacing: 3
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: sensorBackend.telemetryValid ? sensorBackend.stm32Speed.toFixed(1) + " m/s" : "--"; color: "#66dfa9"; font.pixelSize: 15; font.bold: true }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "实时速度"; color: "#78909e"; font.pixelSize: 10 }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            radius: 9; color: "#132631"
                            Column { anchors.centerIn: parent; spacing: 3
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: sensorBackend.telemetryValid ? sensorBackend.stm32Battery + " %" : "--"; color: "#f1c55b"; font.pixelSize: 15; font.bold: true }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "飞行器电量"; color: "#78909e"; font.pixelSize: 10 }
                            }
                        }
                    }

                    TabBar {
                        id: stmTabs
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        spacing: 4

                        background: Rectangle {
                            radius: 8
                            color: "#09131c"
                        }

                        TabButton { text: "遥测参数" }
                        TabButton { text: "实时曲线" }
                        TabButton { text: "告警" }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: stmTabs.currentIndex

                        TelemetryPanel { }

                        Stm32FlightCurve { }

                        Rectangle {
                            radius: 10
                            color: root.warningText === "STM32 系统运行正常"
                                   ? "#102a22" : "#342126"
                            border.color: root.warningText === "STM32 系统运行正常"
                                          ? "#2b7359" : "#78434c"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Text {
                                    text: root.warningText === "STM32 系统运行正常"
                                          ? "✓ STM32 系统运行正常"
                                          : "! STM32 状态提醒"
                                    color: root.warningText === "STM32 系统运行正常"
                                           ? "#6ce1ad" : "#ff8c96"
                                    font.pixelSize: 18
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
                                    text: root.warningText
                                    color: "#d7e3e8"
                                    font.pixelSize: 14
                                    lineHeight: 1.5
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            Stm32MapView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        Rectangle {
            id: flightControlPanel
            Layout.fillWidth: true
            Layout.preferredHeight: 138
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 14
            radius: 12
            color: "#101c26"
            border.color: sensorBackend.manualControl ? "#b9782b" : "#27765f"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 11
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    spacing: 10

                    Text {
                        text: "飞行控制"
                        color: "#f2f6f8"
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Rectangle {
                        Layout.preferredWidth: modeText.implicitWidth + 18
                        Layout.preferredHeight: 23
                        radius: 12
                        color: sensorBackend.manualControl ? "#573b20" : "#173c31"
                        border.color: sensorBackend.manualControl ? "#c98a3d" : "#2c8c69"

                        Text {
                            id: modeText
                            anchors.centerIn: parent
                            text: sensorBackend.manualControl ? "人工接管" : "自主飞行"
                            color: sensorBackend.manualControl ? "#ffc66f" : "#63e2ae"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: sensorBackend.commandStatus
                        color: sensorBackend.telemetryConnected ? "#87a8b8" : "#ef858d"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "命令 JSON / TCP " + sensorBackend.telemetryTarget
                        color: "#617c8c"
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 7

                    Button {
                        id: freeFlightButton
                        Layout.preferredWidth: 112
                        Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "自由飞行\n预设航线"
                        onClicked: sensorBackend.sendFlightCommand("free_flight",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   speedInput.value)

                        contentItem: Text {
                            text: freeFlightButton.text
                            color: freeFlightButton.enabled ? "white" : "#71818a"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 8
                            color: freeFlightButton.down ? "#207354" : "#18553f"
                            border.color: "#3baa7e"
                        }
                    }

                    Button {
                        id: takeoffButton
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "起飞"
                        onClicked: sensorBackend.sendFlightCommand("takeoff",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   Math.max(5, speedInput.value))
                        contentItem: Text { text: takeoffButton.text; color: takeoffButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: takeoffButton.down ? "#278967" : "#1c694e"; border.color: "#3b9a78" }
                    }
                    Button {
                        id: hoverButton
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "悬停"
                        onClicked: sensorBackend.sendFlightCommand("hover",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   0)
                        contentItem: Text { text: hoverButton.text; color: hoverButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: hoverButton.down ? "#32728d" : "#22536a"; border.color: "#4387a4" }
                    }
                    Button {
                        id: loiterButton
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "盘旋"
                        onClicked: sensorBackend.sendFlightCommand("loiter",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   Math.max(10, speedInput.value))
                        contentItem: Text { text: loiterButton.text; color: loiterButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: loiterButton.down ? "#376b9a" : "#284f75"; border.color: "#477baa" }
                    }
                    Button {
                        id: headingButton
                        Layout.preferredWidth: 88; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "改变航向"
                        onClicked: sensorBackend.sendFlightCommand("set_heading",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   Math.max(5, speedInput.value))
                        contentItem: Text { text: headingButton.text; color: headingButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap }
                        background: Rectangle { radius: 8; color: headingButton.down ? "#956b2c" : "#755320"; border.color: "#a57b3a" }
                    }
                    Button {
                        id: rthButton
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "返航"
                        onClicked: sensorBackend.sendFlightCommand("rth",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   speedInput.value)
                        contentItem: Text { text: rthButton.text; color: rthButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: rthButton.down ? "#9b7131" : "#7b5523"; border.color: "#ad8240" }
                    }
                    Button {
                        id: landButton
                        Layout.preferredWidth: 68; Layout.fillHeight: true
                        enabled: sensorBackend.telemetryConnected && !sensorBackend.commandPending
                        text: "降落"
                        onClicked: sensorBackend.sendFlightCommand("land",
                                                                   headingInput.value,
                                                                   altitudeInput.value,
                                                                   0)
                        contentItem: Text { text: landButton.text; color: landButton.enabled ? "white" : "#71818a"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 8; color: landButton.down ? "#a0444c" : "#7a3035"; border.color: "#b4535b" }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        color: "#304754"
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 82
                        Layout.fillHeight: true
                        spacing: 2
                        Text { text: "航向角 °"; color: "#8da2ae"; font.pixelSize: 10 }
                        SpinBox {
                            id: headingInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            from: 0
                            to: 359
                            value: sensorBackend.telemetryValid
                                   ? Math.round(sensorBackend.heading) : 0
                            editable: true
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 86
                        Layout.fillHeight: true
                        spacing: 2
                        Text { text: "目标高度 m"; color: "#8da2ae"; font.pixelSize: 10 }
                        SpinBox {
                            id: altitudeInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            from: 0
                            to: 500
                            value: sensorBackend.telemetryValid
                                   ? Math.round(sensorBackend.targetAltitude) : 50
                            editable: true
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 82
                        Layout.fillHeight: true
                        spacing: 2
                        Text { text: "速度 m/s"; color: "#8da2ae"; font.pixelSize: 10 }
                        SpinBox {
                            id: speedInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            from: 0
                            to: 50
                            value: sensorBackend.telemetryValid
                                   ? Math.round(sensorBackend.stm32Speed) : 5
                            editable: true
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: sensorBackend
        function onTelemetryChanged() { root.updateWarnings() }
        function onTelemetryConnectionChanged() { root.updateWarnings() }
    }

    Component.onCompleted: updateWarnings()
}
