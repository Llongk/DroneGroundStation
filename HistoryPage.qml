import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#0b141d"
    signal backRequested()

    property string selectedSession: ""
    property string selectedSource: ""

    function selectRecord(sessionId, source) {
        selectedSession = sessionId
        selectedSource = source

        var samples = historyDatabase.getSessionData(sessionId, source)
        var latitude = []
        var longitude = []
        var altitude = []
        var temperature = []
        var humidity = []

        for (var i = 0; i < samples.length; ++i) {
            latitude.push(Number(samples[i].latitude))
            longitude.push(Number(samples[i].longitude))
            altitude.push(Number(samples[i].altitude))
            temperature.push(Number(samples[i].temperature))
            humidity.push(Number(samples[i].humidity))
        }

        historyMap.showPath(samples)
        coordinateChart.updateData(latitude, longitude)
        altitudeChart.updateData(altitude)
        environmentChart.updateData(temperature, humidity)
        abnormalPanel.updateData(
                    historyDatabase.getAbnormalData(sessionId, source),
                    sessionId)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#101f2b"
            border.color: "#263f4e"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Button {
                    id: backButton
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 34
                    text: "← 返回主页"
                    onClicked: root.backRequested()

                    contentItem: Text {
                        text: backButton.text
                        color: "#e8f1f5"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: backButton.down ? "#28566a" : "#173848"
                        border.color: "#2c657a"
                    }
                }

                Text {
                    text: "飞行历史数据"
                    color: "#f1f6f8"
                    font.pixelSize: 21
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.selectedSession === ""
                          ? "请选择下方的一条真实记录"
                          : (root.selectedSource === "phone" ? "手机" : "STM32")
                            + " · " + root.selectedSession
                    color: root.selectedSession === "" ? "#8195a2" : "#67d8ee"
                    font.pixelSize: 12
                    font.bold: root.selectedSession !== ""
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 14
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(220, root.height * 0.27)
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 350
                    Layout.fillHeight: true
                    radius: 12
                    color: "#13222d"
                    border.color: "#263d4b"
                    clip: true

                    StatusPanel {
                        id: abnormalPanel
                        anchors.fill: parent
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#13222d"
                    border.color: "#263d4b"
                    clip: true

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
                    Layout.preferredWidth: 350
                    Layout.fillHeight: true
                    radius: 12
                    color: "#16222d"
                    border.color: "#263d4b"
                    clip: true

                    TemperatureHumidityChart {
                        id: environmentChart
                        anchors.fill: parent
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    color: "#16222d"
                    border.color: "#263d4b"
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        spacing: 4

                        CoordinateChart {
                            id: coordinateChart
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.fillHeight: true
                            color: "#29404e"
                        }

                        AltitudeChart {
                            id: altitudeChart
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 190
                radius: 12
                color: "#13222d"
                border.color: "#263d4b"
                clip: true

                HistoryTable {
                    anchors.fill: parent
                    onRecordSelected: function(sessionId, source) {
                        root.selectRecord(sessionId, source)
                    }
                }
            }
        }
    }
}
