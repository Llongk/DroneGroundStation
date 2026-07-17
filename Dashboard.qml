import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

// 飞行状态仪表板，显示飞行参数和基本控制按钮。
Rectangle {
    id: root

    // 飞行状态文本（待命/飞行中/悬停/降落）
    property string flightStatus: "待命"

    border.width: 0 // 无边框

    color: "transparent" // 透明背景

    // 根据内容自动调整高度
    height: layout.height + 24 // 内容高度 + 上下边距
    radius: 8 // 圆角

    ColumnLayout {
        id: layout

        anchors.left: parent.left // 左对齐
        anchors.margins: 12 // 边距
        anchors.right: parent.right // 右对齐
        anchors.top: parent.top // 顶对齐
        spacing: 8 // 元素间距

        // 标题
        Text {
            color: "#00ffff" // 青色
            font.bold: true // 加粗
            font.pixelSize: 18 // 字号
            text: "✈ 飞行状态" // 标题文本
        }

        // 当前状态指示器
        Rectangle {
            Layout.fillWidth: true // 填满宽度
            // 根据状态显示不同颜色：飞行中-绿色，待命-橙色，其他-红色
            color: root.flightStatus === "飞行中" ? "#2e7d32" : root.flightStatus === "待命" ? "#f57f17" :
                                                                                          "#b71c1c"
            height: 30 // 指示器高度
            radius: 5 // 圆角

            Text {
                anchors.centerIn: parent // 居中
                color: "white" // 白色文字
                font.bold: true // 加粗
                font.pixelSize: 14 // 字号
                text: root.flightStatus // 显示当前状态
            }
        }

        // 飞行参数网格
        GridLayout {
            Layout.fillWidth: true // 填满宽度
            columnSpacing: 15 // 列间距
            columns: 2 // 两列
            rowSpacing: 6 // 行间距

            // 高度
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "高度:"
            }
            Text {
                color: "white" // 白色数值
                text: Number(backend.height || 0).toFixed(1) + " m" // 保留 1 位小数
            }
            // 速度
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "速度:"
            }
            Text {
                color: "white" // 白色数值
                text: Number(backend.speed || 0).toFixed(1) + " m/s" // 保留 1 位小数
            }
            // 电量
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "电量:"
            }
            Text {
                color: "white" // 白色数值
                text: Number(backend.battery || 0).toFixed(1) + " %" // 保留 1 位小数
            }
            // 纬度
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "纬度:"
            }
            Text {
                color: "white" // 白色数值
                text: Number(backend.latitude || 0).toFixed(8) // 保留 8 位小数
            }
            // 经度
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "经度:"
            }
            Text {
                color: "white" // 白色数值
                text: Number(backend.longitude || 0).toFixed(8) // 保留 8 位小数
            }
            // GPS 时间
            Text {
                color: "#8a9ba8" // 灰色标签
                text: "时间:"
            }
            Text {
                color: "white" // 白色数值
                text: backend.gpsTime || "--" // 无时间时显示 "--"
            }
        }

        // 控制按钮网格
        GridLayout {
            Layout.fillWidth: true // 填满宽度
            columnSpacing: 8 // 列间距
            columns: 3 // 三列

            // 起飞按钮
            Button {
                Layout.fillWidth: true // 填满列宽
                text: "起飞" // 按钮文本

                background: Rectangle {
                    color: "#2e7d32" // 绿色背景
                    radius: 5 // 圆角
                }

                onClicked: {
                    root.flightStatus = "飞行中"; // 设置飞行状态
                }
            }
            // 悬停按钮
            Button {
                Layout.fillWidth: true // 填满列宽
                text: "悬停" // 按钮文本

                background: Rectangle {
                    color: "#f57f17" // 橙色背景
                    radius: 5 // 圆角
                }

                onClicked: {
                    root.flightStatus = "悬停"; // 设置悬停状态
                }
            }
            // 降落按钮
            Button {
                Layout.fillWidth: true // 填满列宽
                text: "降落" // 按钮文本

                background: Rectangle {
                    color: "#b71c1c" // 红色背景
                    radius: 5 // 圆角
                }

                onClicked: {
                    root.flightStatus = "降落"; // 设置降落状态
                }
            }
        }
    }
}