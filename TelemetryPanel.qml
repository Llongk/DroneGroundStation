import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    readonly property var telemetryFields: [
        {
            label: "设备 ID",
            key: "id"
        },
        {
            label: "协议版本",
            key: "proto"
        },
        {
            label: "数据序列号",
            key: "seq"
        },
        {
            label: "当前纬度",
            key: "lat"
        },
        {
            label: "当前经度",
            key: "lng"
        },
        {
            label: "航向角",
            key: "heading"
        },
        {
            label: "目标高度",
            key: "target_alt"
        },
        {
            label: "飞行速度",
            key: "speed"
        },
        {
            label: "返航点纬度",
            key: "rth_lat"
        },
        {
            label: "返航点经度",
            key: "rth_lng"
        },
        {
            label: "返航距离",
            key: "rth_distance"
        },
        {
            label: "DHT 温度",
            key: "dht_temp"
        },
        {
            label: "DHT 湿度",
            key: "dht_hum"
        },
        {
            label: "SHT 温度",
            key: "sht_temp"
        },
        {
            label: "SHT 湿度",
            key: "sht_hum"
        },
        {
            label: "MCU 温度",
            key: "mcu_temp"
        },
        {
            label: "飞行模式",
            key: "mode"
        },
        {
            label: "告警代码",
            key: "alarm"
        },
        {
            label: "飞行器电量",
            key: "battery"
        },
        {
            label: "设备时间戳",
            key: "ts"
        },
        {
            label: "本地接收时间",
            key: "update_time"
        }
    ]

    // 将遥测字段键映射为带单位的手机实时值。
    function valueFor(key) {
        if (!sensorBackend.telemetryValid)
            return "--";

        switch (key) {
        case "id":
            return sensorBackend.deviceId;
        case "proto":
            return "v" + sensorBackend.protocolVersion;
        case "seq":
            return String(sensorBackend.sequence);
        case "lat":
            return sensorBackend.stm32Latitude.toFixed(6) + "°";
        case "lng":
            return sensorBackend.stm32Longitude.toFixed(6) + "°";
        case "heading":
            return sensorBackend.heading.toFixed(1) + "°";
        case "target_alt":
            return sensorBackend.targetAltitude.toFixed(1) + " m";
        case "speed":
            return sensorBackend.stm32Speed.toFixed(1) + " m/s";
        case "rth_lat":
            return sensorBackend.rthLatitude.toFixed(6) + "°";
        case "rth_lng":
            return sensorBackend.rthLongitude.toFixed(6) + "°";
        case "rth_distance":
            return sensorBackend.rthDistance.toFixed(1) + " m";
        case "dht_temp":
            return sensorBackend.dhtTemperature.toFixed(1) + " ℃";
        case "dht_hum":
            return sensorBackend.dhtHumidity.toFixed(1) + " %";
        case "sht_temp":
            return sensorBackend.shtTemperature.toFixed(1) + " ℃";
        case "sht_hum":
            return sensorBackend.shtHumidity.toFixed(1) + " %";
        case "mcu_temp":
            return sensorBackend.mcuTemperature.toFixed(1) + " ℃";
        case "mode":
            return "代码 " + sensorBackend.flightMode;
        case "alarm":
            return "代码 " + sensorBackend.alarmCode;
        case "battery":
            return sensorBackend.stm32Battery + " %";
        case "ts":
            return String(sensorBackend.telemetryTimestamp);
        case "update_time":
            return sensorBackend.telemetryUpdateTime || "--";
        default:
            return "--";
        }
    }

    color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.maximumHeight: 44
            Layout.minimumHeight: 44
            Layout.preferredHeight: 44
            border.color: sensorBackend.telemetryConnected ? "#2b8065" : "#44515c"
            color: sensorBackend.telemetryConnected ? "#14372e" : "#282f36"
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    Layout.preferredHeight: 9
                    Layout.preferredWidth: 9
                    color: sensorBackend.telemetryConnected ? "#45dda5" : "#7c8993"
                    radius: 5
                }
                Text {
                    Layout.fillWidth: true
                    color: "#e8f1f5"
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 12
                    text: sensorBackend.telemetryConnected ? "遥测接收正常 · "
                                                             + sensorBackend.telemetryPeer :
                                                             sensorBackend.telemetryStatus
                }
                Text {
                    color: "#79d9eb"
                    font.bold: true
                    font.pixelSize: 11
                    text: sensorBackend.telemetryValid ? "帧 #" + sensorBackend.sequence : "尚无有效数据"
                }
            }
        }
        ListView {
            id: parameterList

            Layout.fillHeight: true
            Layout.fillWidth: true
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            model: root.telemetryFields
            spacing: 4

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            delegate: Rectangle {
                id: parameterRow

                required property int index
                required property var modelData

                border.color: "#203b48"
                color: index % 2 === 0 ? "#132631" : "#10222c"
                height: 42
                radius: 7
                width: parameterList.width - 12

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12

                    Text {
                        Layout.preferredWidth: Math.min(150, parameterRow.width * 0.42)
                        color: "#83a0b1"
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: parameterRow.modelData.label
                    }
                    Text {
                        Layout.fillWidth: true
                        color: "#edf5f8"
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight
                        text: root.valueFor(parameterRow.modelData.key)
                    }
                }
            }
        }
    }
}
