import QtQuick
import QtQuick.Controls

// 异常状态面板，展示指定飞行会话的异常点列表，含呼吸闪动边框。
Rectangle {
    id: root

    // 异常数据列表
    property var abnormalList: []
    // 当前飞行记录 ID
    property string flightId: ""
    // 强制刷新计数器
    property int updateFlag: 0
    // 是否有异常数据
    readonly property bool hasAbnormalData: abnormalList && abnormalList.length > 0

    // 根据选中历史会话更新状态摘要和异常列表。
    function updateData(data, id) {
        flightId = id; // 设置飞行记录 ID

        // 先清空
        abnormalList = []; // 清空异常列表

        // 强制刷新
        updateFlag++; // 触发 Repeater 重建

        // 下一帧重新赋值
        if (data) {
            abnormalList = data; // 设置异常数据
        } else {
            abnormalList = []; // 保持空列表
        }
    }

    color: "transparent" // 透明背景

    Column {
        anchors.fill: parent // 填满父容器
        anchors.margins: 20 // 边距
        height: childrenRect.height // 自适应高度
        spacing: 10 // 元素间距

        // 标题
        Text {
            color: "white" // 白色
            font.bold: true // 加粗
            font.pixelSize: 24 // 字号
            text: "轨迹异常状态" // 标题文本
        }
        // 飞行记录 ID
        Text {
            color: "#00ffff" // 青色
            font.pixelSize: 16 // 字号
            text: "飞行记录:" + flightId // 显示记录 ID
        }
        // 异常数量
        Text {
            color: "#ff5555" // 红色
            font.bold: true // 加粗
            font.pixelSize: 18 // 字号
            text: "异常数量:" + abnormalList.length // 显示异常数量
        }
        // 异常列表容器
        Rectangle {
            color: "transparent" // 透明背景
            height: parent.height - 100 // 剩余高度
            width: parent.width // 填满宽度

            ScrollView {
                anchors.fill: parent // 填满父容器

                Column {
                    height: childrenRect.height // 自适应高度
                    spacing: 12 // 元素间距
                    width: parent.width // 填满宽度

                    // 异常列表 Repeater
                    Repeater {
                        model: updateFlag >= 0 ? abnormalList : [] // 绑定异常数据

                        delegate: Rectangle {
                            color: "#223344" // 深色背景
                            height: 110 // 卡片高度
                            radius: 8 // 圆角
                            width: parent.width // 填满宽度

                            Column {
                                anchors.fill: parent // 填满父容器
                                anchors.margins: 10 // 边距
                                spacing: 5 // 行间距

                                // 异常点序号
                                Text {
                                    color: "#ffd54f" // 黄色
                                    font.pixelSize: 16 // 字号
                                    text: "异常点 " + (index + 1) // 序号从 1 开始
                                }
                                // 时间
                                Text {
                                    color: "white" // 白色
                                    text: "时间:" + modelData["time"] // 显示时间
                                }
                                // 事件描述
                                Text {
                                    color: "#ff5555" // 红色
                                    text: "事件:" + modelData["event"] // 显示事件
                                }
                                // 位置
                                Text {
                                    color: "#00ffff" // 青色
                                    text: "位置:" + modelData["latitude"] + ","
                                    + modelData["longitude"] // 显示经纬度
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
        anchors.fill: parent // 覆盖整个面板
        border.color: "#ff4655" // 红色边框
        border.width: 4 // 边框宽度
        color: "transparent" // 透明填充
        opacity: 1 // 初始透明度
        radius: 10 // 圆角
        visible: root.hasAbnormalData // 有异常时可见
        z: 20 // 置于顶层

        SequentialAnimation on opacity {
            loops: Animation.Infinite // 无限循环
            running: root.hasAbnormalData // 有异常时运行

            NumberAnimation {
                duration: 650 // 持续时间
                easing.type: Easing.InOutQuad // 缓动曲线
                from: 0.2 // 最低透明度
                to: 1.0 // 最高透明度
            }
            NumberAnimation {
                duration: 650 // 持续时间
                easing.type: Easing.InOutQuad // 缓动曲线
                from: 1.0 // 最高透明度
                to: 0.2 // 最低透明度
            }
        }
    }
}