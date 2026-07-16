import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Rectangle {
    id: root

    property string flightStatus: "待命"

    border.width: 0

    //========================
    // 透明背景
    //========================

    color: "transparent"

    // 根据内容自动调整高度

    height: layout.height + 24
    radius: 8

    ColumnLayout {
        id: layout

        anchors.left: parent.left
        anchors.margins: 12
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 8

        //========================
        // 标题
        //========================

        Text {
            color: "#00ffff"
            font.bold: true
            font.pixelSize: 18
            text: "✈ 飞行状态"
        }

        //========================
        // 当前状态
        //========================

        Rectangle {
            Layout.fillWidth: true
            color: root.flightStatus === "飞行中" ? "#2e7d32" : root.flightStatus === "待命" ? "#f57f17" :
                                                                                          "#b71c1c"
            height: 30
            radius: 5

            Text {
                anchors.centerIn: parent
                color: "white"
                font.bold: true
                font.pixelSize: 14
                text: root.flightStatus
            }
        }

        //========================
        // 飞行参数
        //========================

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 15
            columns: 2
            rowSpacing: 6

            Text {
                color: "#8a9ba8"
                text: "高度:"
            }
            Text {
                color: "white"
                text: Number(backend.height || 0).toFixed(1) + " m"
            }
            Text {
                color: "#8a9ba8"
                text: "速度:"
            }
            Text {
                color: "white"
                text: Number(backend.speed || 0).toFixed(1) + " m/s"
            }
            Text {
                color: "#8a9ba8"
                text: "电量:"
            }
            Text {
                color: "white"
                text: Number(backend.battery || 0).toFixed(1) + " %"
            }
            Text {
                color: "#8a9ba8"
                text: "纬度:"
            }
            Text {
                color: "white"
                text: Number(backend.latitude || 0).toFixed(8)
            }
            Text {
                color: "#8a9ba8"
                text: "经度:"
            }
            Text {
                color: "white"
                text: Number(backend.longitude || 0).toFixed(8)
            }
            Text {
                color: "#8a9ba8"
                text: "时间:"
            }
            Text {
                color: "white"
                text: backend.gpsTime || "--"
            }
        }

        //========================
        // 控制按钮
        //========================

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: 8
            columns: 3

            Button {
                Layout.fillWidth: true
                text: "起飞"

                background: Rectangle {
                    color: "#2e7d32"
                    radius: 5
                }

                onClicked: {
                    root.flightStatus = "飞行中";
                }
            }
            Button {
                Layout.fillWidth: true
                text: "悬停"

                background: Rectangle {
                    color: "#f57f17"
                    radius: 5
                }

                onClicked: {
                    root.flightStatus = "悬停";
                }
            }
            Button {
                Layout.fillWidth: true
                text: "降落"

                background: Rectangle {
                    color: "#b71c1c"
                    radius: 5
                }

                onClicked: {
                    root.flightStatus = "降落";
                }
            }
        }
    }
}
