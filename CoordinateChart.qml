import QtQuick 2.15

Item {
    id: root
    property var latitude: []
    property var longitude: []

    function updateData(lat, lon) {
        latitude = lat || []
        longitude = lon || []
        canvas.requestPaint()
    }

    function rangeOf(data) {
        if (!data || data.length === 0)
            return { min: 0, max: 1 }
        var minValue = Number(data[0])
        var maxValue = minValue
        for (var i = 1; i < data.length; ++i) {
            var value = Number(data[i])
            minValue = Math.min(minValue, value)
            maxValue = Math.max(maxValue, value)
        }
        if (Math.abs(maxValue - minValue) < 0.000001) {
            minValue -= 0.0005
            maxValue += 0.0005
        }
        return { min: minValue, max: maxValue }
    }

    function drawSeries(ctx, data, range, color, left, right, top, bottom) {
        if (!data || data.length === 0)
            return
        ctx.beginPath()
        ctx.strokeStyle = color
        ctx.lineWidth = 2.5
        for (var i = 0; i < data.length; ++i) {
            var x = data.length === 1 ? (left + right) / 2
                                      : left + i * (right - left) / (data.length - 1)
            var y = bottom - (Number(data[i]) - range.min)
                    * (bottom - top) / (range.max - range.min)
            if (i === 0) ctx.moveTo(x, y)
            else ctx.lineTo(x, y)
        }
        ctx.stroke()
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#16222d"
            ctx.fillRect(0, 0, width, height)

            var left = 55, right = width - 24, top = 48, bottom = height - 38
            ctx.fillStyle = "#f0f5f7"
            ctx.font = "bold 16px Arial"
            ctx.fillText("GPS 坐标变化", 16, 24)

            ctx.font = "11px Arial"
            ctx.fillStyle = "#58d7ed"
            ctx.fillText("● 纬度", Math.max(16, width - 150), 23)
            ctx.fillStyle = "#ffb56b"
            ctx.fillText("● 经度", Math.max(82, width - 82), 23)

            ctx.strokeStyle = "#35505f"
            ctx.lineWidth = 1
            for (var grid = 0; grid <= 4; ++grid) {
                var gy = top + grid * (bottom - top) / 4
                ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(right, gy); ctx.stroke()
            }
            ctx.strokeStyle = "#8296a2"
            ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, bottom)
            ctx.lineTo(right, bottom); ctx.stroke()

            var latRange = root.rangeOf(root.latitude)
            var lonRange = root.rangeOf(root.longitude)
            root.drawSeries(ctx, root.latitude, latRange, "#58d7ed", left, right, top, bottom)
            root.drawSeries(ctx, root.longitude, lonRange, "#ffb56b", left, right, top, bottom)

            ctx.font = "10px Arial"
            ctx.fillStyle = "#58d7ed"
            ctx.fillText("纬 " + latRange.min.toFixed(5) + " ~ " + latRange.max.toFixed(5), left, height - 20)
            ctx.fillStyle = "#ffb56b"
            ctx.fillText("经 " + lonRange.min.toFixed(5) + " ~ " + lonRange.max.toFixed(5), left, height - 7)

            if (root.latitude.length === 0 && root.longitude.length === 0) {
                ctx.fillStyle = "#7e939f"
                ctx.font = "12px Arial"
                ctx.fillText("请选择历史记录", Math.max(left, width / 2 - 45), height / 2)
            }
        }
    }
}
