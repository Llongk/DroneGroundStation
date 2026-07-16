import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5

Item {
    id: root

    // 从历史采样中提取有效坐标并显示完整飞行轨迹。
    function showPath(samples) {
        var coordinates = [];
        for (var i = 0; i < samples.length; ++i) {
            var lat = Number(samples[i].latitude);
            var lon = Number(samples[i].longitude);
            if (isFinite(lat) && isFinite(lon) && Math.abs(lat) <= 90 && Math.abs(lon) <= 180 && !(
                        Math.abs(lat) < 0.000001 && Math.abs(lon) < 0.000001)) {
                coordinates.push(QtPositioning.coordinate(lat, lon));
            }
        }
        route.path = coordinates;
        if (coordinates.length > 0) {
            map.center = coordinates[Math.floor(coordinates.length / 2)];
            map.zoomLevel = coordinates.length === 1 ? 17 : 15;
        }
    }

    Plugin {
        id: mapPlugin

        name: "osm"
    }
    Map {
        id: map

        anchors.fill: parent
        center: QtPositioning.coordinate(39.9042, 116.4074)
        maximumZoomLevel: 20
        minimumZoomLevel: 3
        plugin: mapPlugin
        zoomLevel: 13

        MapPolyline {
            id: route

            line.color: "#ff5f65"
            line.width: 5
            path: []
        }
        MapQuickItem {
            anchorPoint.x: 7
            anchorPoint.y: 7
            coordinate: visible ? route.path[0] : QtPositioning.coordinate()
            visible: route.path.length > 0

            sourceItem: Rectangle {
                border.color: "white"
                border.width: 2
                color: "#45dda5"
                height: 14
                radius: 7
                width: 14
            }
        }
        MapQuickItem {
            anchorPoint.x: 7
            anchorPoint.y: 7
            coordinate: visible ? route.path[route.path.length - 1] : QtPositioning.coordinate()
            visible: route.path.length > 0

            sourceItem: Rectangle {
                border.color: "white"
                border.width: 2
                color: "#ff5f65"
                height: 14
                radius: 7
                width: 14
            }
        }
        MouseArea {
            property real previousX: 0
            property real previousY: 0

            acceptedButtons: Qt.LeftButton
            anchors.fill: parent
            hoverEnabled: true

            // 拖动时平移历史轨迹地图。
            onPositionChanged: function (mouse) {
                if (!pressed)
                    return;
                map.pan(previousX - mouse.x, previousY - mouse.y);
                previousX = mouse.x;
                previousY = mouse.y;
            }
            // 记录地图拖动的起始鼠标位置。
            onPressed: function (mouse) {
                previousX = mouse.x;
                previousY = mouse.y;
            }
            // 根据滚轮方向缩放历史轨迹地图。
            onWheel: function (wheel) {
                map.zoomLevel += wheel.angleDelta.y > 0 ? 0.5 : -0.5;
            }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        color: "#101d27dd"
        height: 38
        radius: 8
        visible: route.path.length === 0
        width: 220

        Text {
            anchors.centerIn: parent
            color: "#a6b6c0"
            font.pixelSize: 12
            text: "该记录暂无有效轨迹坐标"
        }
    }
}
