import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    signal recordSelected(string sessionId, string source)

    property var records: []
    property var filteredRecords: []
    property int currentRow: -1

    function durationText(seconds) {
        var value = Math.max(0, Number(seconds))
        var minutes = Math.floor(value / 60)
        var remain = value % 60
        return minutes > 0 ? minutes + "分 " + remain + "秒" : remain + "秒"
    }

    function batteryText(record) {
        var start = Number(record.startBattery)
        var end = Number(record.endBattery)
        if (!isFinite(start) || !isFinite(end) || (start === 0 && end === 0))
            return "--"
        return Math.max(0, start - end).toFixed(0) + "%"
    }

    function applyFilter() {
        var keyword = searchField.text.trim().toLowerCase()
        if (keyword === "") {
            filteredRecords = records.slice(0)
            return
        }
        var output = []
        for (var i = 0; i < records.length; ++i) {
            var row = records[i]
            var text = (row.sessionId + " " + row.username + " "
                        + row.sourceName + " " + row.startTime).toLowerCase()
            if (text.indexOf(keyword) >= 0)
                output.push(row)
        }
        filteredRecords = output
        currentRow = -1
    }

    function reload() {
        records = historyDatabase.getHistoryRecords()
        applyFilter()
    }

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
                    text: "真实历史记录"
                    color: "white"
                    font.pixelSize: 17
                    font.bold: true
                }

                Text {
                    text: root.filteredRecords.length + " 条"
                    color: "#64d5e9"
                    font.pixelSize: 12
                }

                Item { Layout.fillWidth: true }

                TextField {
                    id: searchField
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 32
                    placeholderText: "搜索会话、用户、来源或日期"
                    color: "white"
                    onTextChanged: root.applyFilter()
                    background: Rectangle {
                        radius: 7
                        color: "#0c1922"
                        border.color: "#2a4657"
                    }
                }

                Button {
                    id: refreshButton
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 32
                    text: "刷新"
                    onClicked: root.reload()
                    contentItem: Text {
                        text: refreshButton.text
                        color: "#e9f2f5"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 7
                        color: refreshButton.down ? "#28566a" : "#173848"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#1b3040"

            Row {
                anchors.fill: parent
                Text { width: parent.width * 0.08; height: parent.height; text: "来源"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.20; height: parent.height; text: "会话编号"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.08; height: parent.height; text: "用户"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.16; height: parent.height; text: "开始时间"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.16; height: parent.height; text: "结束时间"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.09; height: parent.height; text: "时长"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.07; height: parent.height; text: "采样数"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.12; height: parent.height; text: "起点"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
                Text { width: parent.width * 0.04; height: parent.height; text: "状态"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#dce8ed"; font.pixelSize: 11; font.bold: true }
            }
        }

        ListView {
            id: historyList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.filteredRecords
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: rowItem
                required property var modelData
                required property int index
                width: historyList.width - 12
                height: 42
                color: root.currentRow === index ? "#24566c"
                      : (rowMouse.containsMouse ? "#1d3a4b"
                                               : (index % 2 ? "#10202a" : "#132631"))

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.currentRow = rowItem.index
                        root.recordSelected(rowItem.modelData.sessionId,
                                            rowItem.modelData.source)
                    }
                }

                Row {
                    anchors.fill: parent
                    Text { width: parent.width * 0.08; height: parent.height; text: rowItem.modelData.sourceName; color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.20; height: parent.height; text: rowItem.modelData.sessionId; color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.08; height: parent.height; text: rowItem.modelData.username; color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.16; height: parent.height; text: String(rowItem.modelData.startTime).substring(0, 19); color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.16; height: parent.height; text: String(rowItem.modelData.endTime).substring(0, 19); color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.09; height: parent.height; text: root.durationText(rowItem.modelData.durationSeconds); color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.07; height: parent.height; text: rowItem.modelData.sampleCount; color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.12; height: parent.height; text: rowItem.modelData.startPos; color: "#e2edf1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    Text { width: parent.width * 0.04; height: parent.height; text: Number(rowItem.modelData.alarmCount) > 0 ? "警" : "正常"; color: text === "警" ? "#ff7e86" : "#e2edf1"; font.pixelSize: 11; font.bold: text === "警"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: historyList.count === 0
                text: "暂无登录后的真实采集数据"
                color: "#8296a2"
                font.pixelSize: 13
            }
        }
    }

    Connections {
        target: historyDatabase
        function onHistoryChanged() {
            refreshTimer.restart()
        }
    }

    Timer {
        id: refreshTimer
        interval: 800
        repeat: false
        onTriggered: root.reload()
    }

    Component.onCompleted: reload()
}
