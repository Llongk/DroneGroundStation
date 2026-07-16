import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    property int currentRow: -1
    property var filteredRecords: []
    property var records: []

    signal recordSelected(string sessionId, string source)

    // 根据用户、设备来源或会话关键字过滤历史摘要。
    function applyFilter() {
        var keyword = searchField.text.trim().toLowerCase();
        if (keyword === "") {
            filteredRecords = records.slice(0);
            return;
        }
        var output = [];
        for (var i = 0; i < records.length; ++i) {
            var row = records[i];
            var text = (row.sessionId + " " + row.username + " " + row.sourceName + " "
                        + row.startTime).toLowerCase();
            if (text.indexOf(keyword) >= 0)
                output.push(row);
        }
        filteredRecords = output;
        currentRow = -1;
    }
    // 将会话起止电量格式化为表格显示文本。
    function batteryText(record) {
        var start = Number(record.startBattery);
        var end = Number(record.endBattery);
        if (!isFinite(start) || !isFinite(end) || (start === 0 && end === 0))
            return "--";
        return Math.max(0, start - end).toFixed(0) + "%";
    }
    // 将持续秒数格式化为时分秒文本。
    function durationText(seconds) {
        var value = Math.max(0, Number(seconds));
        var minutes = Math.floor(value / 60);
        var remain = value % 60;
        return minutes > 0 ? minutes + "分 " + remain + "秒" : remain + "秒";
    }
    // 从 C++ 数据库后端重新读取并排序历史会话。
    function reload() {
        records = historyDatabase.getHistoryRecords();
        applyFilter();
    }

    Component.onCompleted: reload()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "#13222d"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Text {
                    color: "white"
                    font.bold: true
                    font.pixelSize: 17
                    text: "真实历史记录"
                }
                Text {
                    color: "#64d5e9"
                    font.pixelSize: 12
                    text: root.filteredRecords.length + " 条"
                }
                Item {
                    Layout.fillWidth: true
                }
                TextField {
                    id: searchField

                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 260
                    color: "white"
                    placeholderText: "搜索会话、用户、来源或日期"

                    background: Rectangle {
                        border.color: "#2a4657"
                        color: "#0c1922"
                        radius: 7
                    }

                    onTextChanged: root.applyFilter()
                }
                Button {
                    id: refreshButton

                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 72
                    text: "刷新"

                    background: Rectangle {
                        color: refreshButton.down ? "#28566a" : "#173848"
                        radius: 7
                    }
                    contentItem: Text {
                        color: "#e9f2f5"
                        horizontalAlignment: Text.AlignHCenter
                        text: refreshButton.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.reload()
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#1b3040"

            Row {
                anchors.fill: parent

                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "来源"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.08
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "会话编号"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.20
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "用户"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.08
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "开始时间"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.16
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "结束时间"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.16
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "时长"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.09
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "采样数"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.07
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "起点"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.12
                }
                Text {
                    color: "#dce8ed"
                    font.bold: true
                    font.pixelSize: 11
                    height: parent.height
                    horizontalAlignment: Text.AlignHCenter
                    text: "状态"
                    verticalAlignment: Text.AlignVCenter
                    width: parent.width * 0.04
                }
            }
        }
        ListView {
            id: historyList

            Layout.fillHeight: true
            Layout.fillWidth: true
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            model: root.filteredRecords

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            delegate: Rectangle {
                id: rowItem

                required property int index
                required property var modelData

                color: root.currentRow === index ? "#24566c" : (rowMouse.containsMouse ? "#1d3a4b" :
                                                                                         (index % 2
                                                                                          ? "#10202a" :
                                                                                            "#132631"))
                height: 42
                width: historyList.width - 12

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: {
                        root.currentRow = rowItem.index;
                        root.recordSelected(rowItem.modelData.sessionId, rowItem.modelData.source);
                    }
                }
                Row {
                    anchors.fill: parent

                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: rowItem.modelData.sourceName
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.08
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: rowItem.modelData.sessionId
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.20
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: rowItem.modelData.username
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.08
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: String(rowItem.modelData.startTime).substring(0, 19)
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.16
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: String(rowItem.modelData.endTime).substring(0, 19)
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.16
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: root.durationText(rowItem.modelData.durationSeconds)
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.09
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: rowItem.modelData.sampleCount
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.07
                    }
                    Text {
                        color: "#e2edf1"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: rowItem.modelData.startPos
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.12
                    }
                    Text {
                        color: text === "警" ? "#ff7e86" : "#e2edf1"
                        elide: Text.ElideRight
                        font.bold: text === "警"
                        font.pixelSize: 11
                        height: parent.height
                        horizontalAlignment: Text.AlignHCenter
                        text: Number(rowItem.modelData.alarmCount) > 0 ? "警" : "正常"
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width * 0.04
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                color: "#8296a2"
                font.pixelSize: 13
                text: "暂无登录后的真实采集数据"
                visible: historyList.count === 0
            }
        }
    }
    Connections {
        // 数据库写入后刷新历史表格。
        function onHistoryChanged() {
            refreshTimer.restart();
        }

        target: historyDatabase
    }
    Timer {
        id: refreshTimer

        interval: 800
        repeat: false

        onTriggered: root.reload()
    }
}
