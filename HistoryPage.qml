import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string selectedSession: ""
    property string selectedSource: ""

    signal backRequested

    // 载入选中会话的明细、异常点、轨迹和各类曲线。
    function selectRecord(sessionId, source) {
        selectedSession = sessionId;
        selectedSource = source;

        var samples = historyDatabase.getSessionData(sessionId, source);
        var latitude = [];
        var longitude = [];
        var altitude = [];
        var temperature = [];
        var humidity = [];

        for (var i = 0; i < samples.length; ++i) {
            latitude.push(Number(samples[i].latitude));
            longitude.push(Number(samples[i].longitude));
            altitude.push(Number(samples[i].altitude));
            temperature.push(Number(samples[i].temperature));
            humidity.push(Number(samples[i].humidity));
        }

        historyMap.showPath(samples);
        coordinateChart.updateData(latitude, longitude);
        altitudeChart.updateData(altitude);
        environmentChart.updateData(temperature, humidity);
        abnormalPanel.updateData(historyDatabase.getAbnormalData(sessionId, source), sessionId);
    }

    color: "#0b141d"

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            border.color: "#263f4e"
            color: "#101f2b"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Button {
                    id: backButton

                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 92
                    text: "← 返回主页"

                    background: Rectangle {
                        border.color: "#2c657a"
                        color: backButton.down ? "#28566a" : "#173848"
                        radius: 8
                    }
                    contentItem: Text {
                        color: "#e8f1f5"
                        font.bold: true
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        text: backButton.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.backRequested()
                }
                Text {
                    color: "#f1f6f8"
                    font.bold: true
                    font.pixelSize: 21
                    text: "飞行历史数据"
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    color: root.selectedSession === "" ? "#8195a2" : "#67d8ee"
                    font.bold: root.selectedSession !== ""
                    font.pixelSize: 12
                    text: root.selectedSession === "" ? "请选择下方的一条真实记录" : (root.selectedSource
                                                                          === "phone" ? "手机" :
                                                                                        "STM32")
                                                        + " · " + root.selectedSession
                }
            }
        }
        ColumnLayout {
            Layout.bottomMargin: 14
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(220, root.height * 0.27)
                spacing: 12

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 350
                    border.color: "#263d4b"
                    clip: true
                    color: "#13222d"
                    radius: 12

                    StatusPanel {
                        id: abnormalPanel

                        anchors.fill: parent
                    }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    border.color: "#263d4b"
                    clip: true
                    color: "#13222d"
                    radius: 12

                    HistoryFlightPath {
                        id: historyMap

                        anchors.fill: parent
                        anchors.margins: 1
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(210, root.height * 0.27)
                spacing: 12

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 350
                    border.color: "#263d4b"
                    clip: true
                    color: "#16222d"
                    radius: 12

                    TemperatureHumidityChart {
                        id: environmentChart

                        anchors.fill: parent
                    }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    border.color: "#263d4b"
                    clip: true
                    color: "#16222d"
                    radius: 12

                    RowLayout {
                        anchors.fill: parent
                        spacing: 4

                        CoordinateChart {
                            id: coordinateChart

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 1
                            color: "#29404e"
                        }
                        AltitudeChart {
                            id: altitudeChart

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: 190
                border.color: "#263d4b"
                clip: true
                color: "#13222d"
                radius: 12

                HistoryTable {
                    anchors.fill: parent

                    // 表格选择变化时切换当前历史会话。
                    onRecordSelected: function (sessionId, source) {
                        root.selectRecord(sessionId, source);
                    }
                }
            }
        }
    }
}
