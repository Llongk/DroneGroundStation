import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property var abnormalList: []
    property string flightId: ""
    property int updateFlag: 0
    readonly property bool hasAbnormalData: abnormalList && abnormalList.length > 0

    // 根据选中历史会话更新状态摘要和异常列表。
    function updateData(data, id) {
        flightId = id;

        // 先清空

        abnormalList = [];

        // 强制刷新

        updateFlag++;

        // 下一帧重新赋值

        if (data) {
            abnormalList = data;
        } else {
            abnormalList = [];
        }
    }

    color: "transparent"

    Column {
        anchors.fill: parent
        anchors.margins: 20
        height: childrenRect.height
        spacing: 10

        Text {
            color: "white"
            font.bold: true
            font.pixelSize: 24
            text: "轨迹异常状态"
        }
        Text {
            color: "#00ffff"
            font.pixelSize: 16
            text: "飞行记录:" + flightId
        }
        Text {
            color: "#ff5555"
            font.bold: true
            font.pixelSize: 18
            text: "异常数量:" + abnormalList.length
        }
        Rectangle {
            color: "transparent"
            height: parent.height - 100
            width: parent.width

            ScrollView {
                anchors.fill: parent

                Column {
                    height: childrenRect.height
                    spacing: 12
                    width: parent.width

                    Repeater {
                        model: updateFlag >= 0 ? abnormalList : []

                        delegate: Rectangle {
                            color: "#223344"
                            height: 110
                            radius: 8
                            width: parent.width

                            Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 5

                                Text {
                                    color: "#ffd54f"
                                    font.pixelSize: 16
                                    text: "异常点 " + (index + 1)
                                }
                                Text {
                                    color: "white"
                                    text: "时间:" + modelData["time"]
                                }
                                Text {
                                    color: "#ff5555"
                                    text: "事件:" + modelData["event"]
                                }
                                Text {
                                    color: "#00ffff"
                                    text: "位置:" + modelData["latitude"] + ","
                                    + modelData["longitude"]
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 选中的历史会话存在异常点时，面板边缘以较慢节奏闪动。
    Rectangle {
        anchors.fill: parent
        border.color: "#ff4655"
        border.width: 4
        color: "transparent"
        opacity: 1
        radius: 10
        visible: root.hasAbnormalData
        z: 20

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: root.hasAbnormalData

            NumberAnimation {
                duration: 650
                easing.type: Easing.InOutQuad
                from: 0.2
                to: 1.0
            }
            NumberAnimation {
                duration: 650
                easing.type: Easing.InOutQuad
                from: 1.0
                to: 0.2
            }
        }
    }
}
