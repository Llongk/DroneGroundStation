import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// STM32 遥测数据面板，显示所有遥测字段的实时值。
Rectangle {
    id: root

    // 遥测字段定义：标签与键名映射
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
        if (!sensorBackend.telemetryValid) // 遥测无效时返回 "--"
            return "--";

        switch (key) {
        case "id":
            return sensorBackend.deviceId; // 设备 ID
        case "proto":
            return "v" + sensorBackend.protocolVersion; // 协议版本
        case "seq":
            return String(sensorBackend.sequence); // 序列号
        case "lat":
            return sensorBackend.stm32Latitude.toFixed(6) + "°"; // 纬度
        case "lng":
            return sensorBackend.stm32Longitude.toFixed(6) + "°"; // 经度
        case "heading":
            return sensorBackend.heading.toFixed(1) + "°"; // 航向角
        case "target_alt":
            return sensorBackend.targetAltitude.toFixed(1) + " m"; // 目标高度
        case "speed":
            return sensorBackend.stm32Speed.toFixed(1) + " m/s"; // 飞行速度
        case "rth_lat":
            return sensorBackend.rthLatitude.toFixed(6) + "°"; // 返航点纬度
        case "rth_lng":
            return sensorBackend.rthLongitude.toFixed(6) + "°"; // 返航点经度
        case "rth_distance":
            return sensorBackend.rthDistance.toFixed(1) + " m"; // 返航距离
        case "dht_temp":
            return sensorBackend.dhtTemperature.toFixed(1) + " ℃"; // DHT 温度
        case "dht_hum":
            return sensorBackend.dhtHumidity.toFixed(1) + " %"; // DHT 湿度
        case "sht_temp":
            return sensorBackend.shtTemperature.toFixed(1) + " ℃"; // SHT 温度
        case "sht_hum":
            return sensorBackend.shtHumidity.toFixed(1) + " %"; // SHT 湿度
        case "mcu_temp":
            return sensorBackend.mcuTemperature.toFixed(1) + " ℃"; // MCU 温度
        case "mode":
            return "代码 " + sensorBackend.flightMode; // 飞行模式代码
        case "alarm":
            return "代码 " + sensorBackend.alarmCode; // 告警代码
        case "battery":
            return sensorBackend.stm32Battery + " %"; // 飞行器电量
        case "ts":
            return String(sensorBackend.telemetryTimestamp); // 设备时间戳
        case "update_time":
            return sensorBackend.telemetryUpdateTime || "--"; // 本地接收时间
        default:
            return "--"; // 未知字段
        }
    }

    color: "transparent" // 透明背景

    ColumnLayout {
        anchors.fill: parent // 填满父容器
        anchors.margins: 10 // 边距
        spacing: 8 // 元素间距

        // 遥测连接状态条
        Rectangle {
            Layout.fillWidth: true // 填满宽度
            Layout.maximumHeight: 44 // 最大高度
            Layout.minimumHeight: 44 // 最小高度
            Layout.preferredHeight: 44 // 首选高度
            border.color: sensorBackend.telemetryConnected ? "#2b8065" : "#44515c" // 连接时绿色边框
            color: sensorBackend.telemetryConnected ? "#14372e" : "#282f36" // 连接时深绿背景
            radius: 8 // 圆角

            RowLayout {
                anchors.fill: parent // 填满父容器
                anchors.leftMargin: 12 // 左边距
                anchors.rightMargin: 12 // 右边距
                spacing: 8 // 间距

                // 状态指示灯
                Rectangle {
                    Layout.preferredHeight: 9 // 高度
                    Layout.preferredWidth: 9 // 宽度
                    color: sensorBackend.telemetryConnected ? "#45dda5" : "#7c8993" // 连接时绿色
                    radius: 5 // 圆角
                }
                // 状态文本
                Text {
                    Layout.fillWidth: true // 填满宽度
                    color: "#e8f1f5" // 浅色文字
                    elide: Text.ElideRight // 溢出省略
                    font.bold: true // 加粗
                    font.pixelSize: 12 // 字号
                    text: sensorBackend.telemetryConnected ? "遥测接收正常 · "
                                                             + sensorBackend.telemetryPeer :
                                                             sensorBackend.telemetryStatus // 连接状态
                }
                // 帧序号
                Text {
                    color: "#79d9eb" // 青色
                    font.bold: true // 加粗
                    font.pixelSize: 11 // 字号
                    text: sensorBackend.telemetryValid ? "帧 #" + sensorBackend.sequence : "尚无有效数据" // 帧序号
                }
            }
        }
        // 遥测参数列表
        ListView {
            id: parameterList

            Layout.fillHeight: true // 填满高度
            Layout.fillWidth: true // 填满宽度
            boundsBehavior: Flickable.StopAtBounds // 边界停止
            clip: true // 裁剪溢出
            model: root.telemetryFields // 绑定遥测字段
            spacing: 4 // 行间距

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded // 按需显示滚动条
            }
            delegate: Rectangle {
                id: parameterRow

                required property int index // 行索引
                required property var modelData // 字段数据

                border.color: "#203b48" // 边框颜色
                color: index % 2 === 0 ? "#132631" : "#10222c" // 交替行颜色
                height: 42 // 行高
                radius: 7 // 圆角
                width: parameterList.width - 12 // 留出滚动条空间

                RowLayout {
                    anchors.fill: parent // 填满父容器
                    anchors.leftMargin: 12 // 左边距
                    anchors.rightMargin: 12 // 右边距
                    spacing: 12 // 间距

                    // 字段标签
                    Text {
                        Layout.preferredWidth: Math.min(150, parameterRow.width * 0.42) // 最大宽度
                        color: "#83a0b1" // 灰色
                        elide: Text.ElideRight // 溢出省略
                        font.pixelSize: 12 // 字号
                        text: parameterRow.modelData.label // 显示标签
                    }
                    // 字段值
                    Text {
                        Layout.fillWidth: true // 填满剩余宽度
                        color: "#edf5f8" // 浅色
                        elide: Text.ElideRight // 溢出省略
                        font.bold: true // 加粗
                        font.pixelSize: 13 // 字号
                        horizontalAlignment: Text.AlignRight // 右对齐
                        text: root.valueFor(parameterRow.modelData.key) // 获取对应值
                    }
                }
            }
        }
    }
}