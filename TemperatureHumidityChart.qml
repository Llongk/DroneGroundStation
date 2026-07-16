import QtQuick 2.15

Item {
    id: root

    property var humidityData: []
    property var temperatureData: []

    // 按统一坐标范围绘制温度或湿度折线。
    function drawSeries(ctx, data, minValue, maxValue, color, left, right, top, bottom) {
        if (!data || data.length === 0)
            return;
        ctx.beginPath();
        ctx.strokeStyle = color;
        ctx.lineWidth = 2.5;
        for (var i = 0; i < data.length; ++i) {
            var x = data.length === 1 ? (left + right) / 2 : left + i * (right - left) / (
                                            data.length - 1);
            var y = bottom - (Number(data[i]) - minValue) * (bottom - top) / (maxValue - minValue);
            if (i === 0)
                ctx.moveTo(x, y);
            else
                ctx.lineTo(x, y);
        }
        ctx.stroke();
    }
    // 更新温湿度历史序列并请求重绘。
    function updateData(temperature, humidity) {
        temperatureData = temperature || [];
        humidityData = humidity || [];
        canvas.requestPaint();
    }

    Canvas {
        id: canvas

        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.fillStyle = "#16222d";
            ctx.fillRect(0, 0, width, height);
            ctx.fillStyle = "#f0f5f7";
            ctx.font = "bold 16px Arial";
            ctx.fillText("温湿度历史曲线", 16, 24);

            ctx.font = "11px Arial";
            ctx.fillStyle = "#ff9c63";
            ctx.fillText("● 温度 ℃", 18, 43);
            ctx.fillStyle = "#54d5eb";
            ctx.fillText("● 湿度 %", 95, 43);

            var left = 42, right = width - 20, top = 58, bottom = height - 30;
            ctx.strokeStyle = "#35505f";
            ctx.lineWidth = 1;
            for (var grid = 0; grid <= 4; ++grid) {
                var y = top + grid * (bottom - top) / 4;
                ctx.beginPath();
                ctx.moveTo(left, y);
                ctx.lineTo(right, y);
                ctx.stroke();
                ctx.fillStyle = "#8296a2";
                ctx.font = "10px Arial";
                ctx.fillText(String(100 - grid * 25), 8, y + 3);
            }

            root.drawSeries(ctx, root.temperatureData, -50, 100, "#ff9c63", left, right, top,
                            bottom);
            root.drawSeries(ctx, root.humidityData, 0, 100, "#54d5eb", left, right, top, bottom);

            if (root.temperatureData.length === 0 && root.humidityData.length === 0) {
                ctx.fillStyle = "#7e939f";
                ctx.font = "12px Arial";
                ctx.fillText("请选择历史记录", Math.max(left, width / 2 - 45), height / 2);
            }
        }
    }
}
