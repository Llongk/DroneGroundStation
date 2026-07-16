import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "transparent"

    property var dataPoints: []
    property int selectedIndex: 0

    readonly property var series: [
        { name: "目标高度", key: "alt", unit: "m", color: "#50d7ee" },
        { name: "速度", key: "speed", unit: "m/s", color: "#65e0a8" },
        { name: "返航距离", key: "rth", unit: "m", color: "#f4c95d" },
        { name: "SHT 温度", key: "shtTemp", unit: "℃", color: "#ff9f68" },
        { name: "SHT 湿度", key: "shtHum", unit: "%", color: "#67b7ff" },
        { name: "DHT 温度", key: "dhtTemp", unit: "℃", color: "#ff7d8b" },
        { name: "DHT 湿度", key: "dhtHum", unit: "%", color: "#9b8cff" },
        { name: "MCU 温度", key: "mcuTemp", unit: "℃", color: "#e989df" },
        { name: "电池", key: "battery", unit: "%", color: "#a8dc62" }
    ]

    function addTelemetryPoint() {
        if (!sensorBackend.telemetryValid) {
            return
        }

        var points = dataPoints.slice(0)
        points.push({
            alt: sensorBackend.targetAltitude,
            speed: sensorBackend.stm32Speed,
            rth: sensorBackend.rthDistance,
            shtTemp: sensorBackend.shtTemperature,
            shtHum: sensorBackend.shtHumidity,
            dhtTemp: sensorBackend.dhtTemperature,
            dhtHum: sensorBackend.dhtHumidity,
            mcuTemp: sensorBackend.mcuTemperature,
            battery: sensorBackend.stm32Battery
        })
        if (points.length > 240) {
            points.shift()
        }
        dataPoints = points
        chart.requestPaint()
    }

    Connections {
        target: sensorBackend
        function onTelemetryChanged() { root.addTelemetryPoint() }
    }

    Component.onCompleted: {
        if (sensorBackend.telemetryValid) root.addTelemetryPoint()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "STM32 实时曲线"
                color: "#e8f1f5"
                font.pixelSize: 16
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            ComboBox {
                id: seriesSelector
                Layout.preferredWidth: 130
                model: root.series.map(function(item) { return item.name })
                onCurrentIndexChanged: {
                    root.selectedIndex = currentIndex
                    chart.requestPaint()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#0b1720"
            border.color: "#223946"

            Canvas {
                id: chart
                anchors.fill: parent
                anchors.margins: 10

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.clearRect(0, 0, width, height)

                    var left = 54
                    var right = 18
                    var top = 24
                    var bottom = 34
                    var plotWidth = Math.max(1, width - left - right)
                    var plotHeight = Math.max(1, height - top - bottom)
                    var item = root.series[root.selectedIndex]
                    var points = root.dataPoints

                    ctx.strokeStyle = "#29414f"
                    ctx.fillStyle = "#78909f"
                    ctx.font = "11px sans-serif"
                    ctx.lineWidth = 1

                    if (points.length === 0) {
                        ctx.fillStyle = "#718899"
                        ctx.font = "14px sans-serif"
                        ctx.fillText("等待 STM32 遥测数据", left + 12, top + 28)
                        return
                    }

                    var minValue = points[0][item.key]
                    var maxValue = minValue
                    for (var i = 1; i < points.length; ++i) {
                        minValue = Math.min(minValue, points[i][item.key])
                        maxValue = Math.max(maxValue, points[i][item.key])
                    }
                    if (Math.abs(maxValue - minValue) < 0.01) {
                        minValue -= 1
                        maxValue += 1
                    } else {
                        var margin = (maxValue - minValue) * 0.12
                        minValue -= margin
                        maxValue += margin
                    }

                    for (var grid = 0; grid <= 5; ++grid) {
                        var y = top + plotHeight * grid / 5
                        var labelValue = maxValue - (maxValue - minValue) * grid / 5
                        ctx.beginPath()
                        ctx.moveTo(left, y)
                        ctx.lineTo(width - right, y)
                        ctx.stroke()
                        ctx.fillText(labelValue.toFixed(1), 4, y + 4)
                    }

                    ctx.fillText(item.name + " (" + item.unit + ")", left, 14)
                    ctx.strokeStyle = item.color
                    ctx.lineWidth = 2.2
                    ctx.beginPath()
                    for (var p = 0; p < points.length; ++p) {
                        var x = points.length === 1
                                ? left
                                : left + plotWidth * p / (points.length - 1)
                        var value = points[p][item.key]
                        var pointY = top + (maxValue - value)
                                     / (maxValue - minValue) * plotHeight
                        if (p === 0) ctx.moveTo(x, pointY)
                        else ctx.lineTo(x, pointY)
                    }
                    ctx.stroke()
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignRight
            text: root.dataPoints.length > 0
                  ? "当前："
                    + root.dataPoints[root.dataPoints.length - 1]
                      [root.series[root.selectedIndex].key].toFixed(1)
                    + " " + root.series[root.selectedIndex].unit
                  : "当前：--"
            color: "#d9e6eb"
            font.pixelSize: 13
        }
    }
}
