import QtQuick 2.15

Item {
    id: root
    property var heightData: []

    function updateData(data) {
        heightData = data || []
        canvas.requestPaint()
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#16222d"
            ctx.fillRect(0, 0, width, height)
            ctx.fillStyle = "#f0f5f7"
            ctx.font = "bold 16px Arial"
            ctx.fillText("飞行高度", 16, 24)

            var left = 55, right = width - 24, top = 42, bottom = height - 34
            var minValue = 0, maxValue = 1
            if (root.heightData.length > 0) {
                minValue = Number(root.heightData[0])
                maxValue = minValue
                for (var i = 1; i < root.heightData.length; ++i) {
                    minValue = Math.min(minValue, Number(root.heightData[i]))
                    maxValue = Math.max(maxValue, Number(root.heightData[i]))
                }
                var padding = Math.max(1, (maxValue - minValue) * 0.12)
                minValue -= padding
                maxValue += padding
            }

            ctx.font = "10px Arial"
            for (var grid = 0; grid <= 4; ++grid) {
                var y = top + grid * (bottom - top) / 4
                var label = maxValue - grid * (maxValue - minValue) / 4
                ctx.strokeStyle = "#35505f"
                ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke()
                ctx.fillStyle = "#8aa0ad"
                ctx.fillText(label.toFixed(1), 8, y + 3)
            }

            if (root.heightData.length > 0) {
                ctx.beginPath()
                ctx.strokeStyle = "#ff6b70"
                ctx.lineWidth = 2.5
                for (var point = 0; point < root.heightData.length; ++point) {
                    var x = root.heightData.length === 1 ? (left + right) / 2
                            : left + point * (right - left) / (root.heightData.length - 1)
                    var py = bottom - (Number(root.heightData[point]) - minValue)
                             * (bottom - top) / (maxValue - minValue)
                    if (point === 0) ctx.moveTo(x, py)
                    else ctx.lineTo(x, py)
                }
                ctx.stroke()
            } else {
                ctx.fillStyle = "#7e939f"
                ctx.font = "12px Arial"
                ctx.fillText("请选择历史记录", Math.max(left, width / 2 - 45), height / 2)
            }

            ctx.fillStyle = "#8aa0ad"
            ctx.font = "10px Arial"
            ctx.fillText("高度 / m", left, height - 10)
        }
    }
}
