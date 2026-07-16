import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5

Item {
    id: root

    function showPath(samples) {
        var coordinates = []
        for (var i = 0; i < samples.length; ++i) {
            var lat = Number(samples[i].latitude)
            var lon = Number(samples[i].longitude)
            if (isFinite(lat) && isFinite(lon)
                    && Math.abs(lat) <= 90 && Math.abs(lon) <= 180
                    && !(Math.abs(lat) < 0.000001 && Math.abs(lon) < 0.000001)) {
                coordinates.push(QtPositioning.coordinate(lat, lon))
            }
        }
        route.path = coordinates
        if (coordinates.length > 0) {
            map.center = coordinates[Math.floor(coordinates.length / 2)]
            map.zoomLevel = coordinates.length === 1 ? 17 : 15
        }
    }

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(39.9042, 116.4074)
        zoomLevel: 13
        minimumZoomLevel: 3
        maximumZoomLevel: 20

        MapPolyline {
            id: route
            line.width: 5
            line.color: "#ff5f65"
            path: []
        }

        MapQuickItem {
            visible: route.path.length > 0
            coordinate: visible ? route.path[0] : QtPositioning.coordinate()
            anchorPoint.x: 7
            anchorPoint.y: 7
            sourceItem: Rectangle {
                width: 14; height: 14; radius: 7
                color: "#45dda5"
                border.color: "white"
                border.width: 2
            }
        }

        MapQuickItem {
            visible: route.path.length > 0
            coordinate: visible ? route.path[route.path.length - 1]
                                : QtPositioning.coordinate()
            anchorPoint.x: 7
            anchorPoint.y: 7
            sourceItem: Rectangle {
                width: 14; height: 14; radius: 7
                color: "#ff5f65"
                border.color: "white"
                border.width: 2
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            property real previousX: 0
            property real previousY: 0

            onPressed: function(mouse) {
                previousX = mouse.x
                previousY = mouse.y
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                map.pan(previousX - mouse.x, previousY - mouse.y)
                previousX = mouse.x
                previousY = mouse.y
            }
            onWheel: function(wheel) {
                map.zoomLevel += wheel.angleDelta.y > 0 ? 0.5 : -0.5
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 220
        height: 38
        radius: 8
        color: "#101d27dd"
        visible: route.path.length === 0

        Text {
            anchors.centerIn: parent
            text: "该记录暂无有效轨迹坐标"
            color: "#a6b6c0"
            font.pixelSize: 12
        }
    }
}
