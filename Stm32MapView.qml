import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 6.5
import QtPositioning 6.5

Rectangle {
    id: root

    property var currentCoordinate: QtPositioning.coordinate(39.9042, 116.4074)
    property bool followDrone: true
    property bool hasPosition: false
    property bool hasRthPosition: false
    property int lastTelemetrySequence: -1
    property var rthCoordinate: QtPositioning.coordinate(0, 0)
    property var trajectory: []

    // 使用最新有效 STM32 坐标更新标记、轨迹和跟随中心。
    function updatePosition() {
        if (!validCoordinate(sensorBackend.stm32Latitude, sensorBackend.stm32Longitude)) {
            return;
        }

        var coordinate = QtPositioning.coordinate(sensorBackend.stm32Latitude,
                                                  sensorBackend.stm32Longitude);
        if (!hasPosition) {
            trajectory = [];
            map.zoomLevel = 18;
        }
        hasPosition = true;
        currentCoordinate = coordinate;

        var points = trajectory.slice(0);
        var shouldAppend = points.length === 0;
        if (!shouldAppend) {
            var distance = points[points.length - 1].distanceTo(coordinate);
            // STM32 telemetry is emitted with six decimal places.  Keep every
            // real coordinate change instead of waiting for a two-metre jump.
            shouldAppend = isFinite(distance) && distance >= 0.05;
        }
        if (shouldAppend) {
            points.push(coordinate);
            route.addCoordinate(coordinate);
            if (points.length > 1000) {
                points.shift();
                route.removeCoordinate(0);
            }
            trajectory = points;
        }
        lastTelemetrySequence = sensorBackend.sequence;

        if (validCoordinate(sensorBackend.rthLatitude, sensorBackend.rthLongitude)) {
            hasRthPosition = true;
            rthCoordinate = QtPositioning.coordinate(sensorBackend.rthLatitude,
                                                     sensorBackend.rthLongitude);
        } else {
            hasRthPosition = false;
        }

        if (followDrone)
            map.center = coordinate;
    }
    // 判断 STM32 坐标是否为可绘制的有效地理位置。
    function validCoordinate(lat, lng) {
        return isFinite(lat) && isFinite(lng) && lat >= -90 && lat <= 90 && lng >= -180 && lng
                <= 180 && !(Math.abs(lat) < 0.000001 && Math.abs(lng) < 0.000001);
    }

    clip: true
    color: "#101a24"
    radius: 12

    Component.onCompleted: {
        if (sensorBackend.telemetryValid)
            updatePosition();
    }

    Connections {
        // 每次有效遥测到达后更新 STM32 地图。
        function onTelemetryChanged() {
            root.updatePosition();
        }

        target: sensorBackend
    }
    Rectangle {
        id: mapHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#14232e"
        height: 48
        z: 5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14

            Text {
                color: "#f0c65b"
                font.bold: true
                font.pixelSize: 17
                text: "STM32 实时航线"
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                color: root.hasPosition ? "#dce9ee" : "#748b99"
                font.pixelSize: 12
                text: root.hasPosition ? root.currentCoordinate.latitude.toFixed(6) + ", "
                                         + root.currentCoordinate.longitude.toFixed(6) : "等待有效坐标"
            }
        }
    }
    Map {
        id: map

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mapHeader.bottom
        center: root.currentCoordinate
        maximumZoomLevel: 20
        minimumZoomLevel: 3
        zoomLevel: 18

        plugin: Plugin {
            name: "osm"
        }

        DragHandler {
            id: stmMapDragHandler

            target: null

            onActiveChanged: {
                if (active)
                    root.followDrone = false;
            }
            // 将拖动手势位移转换为 STM32 地图平移。
            onTranslationChanged: function (delta) {
                map.pan(-delta.x, -delta.y);
            }
        }
        PinchHandler {
            id: stmMapPinchHandler

            property var startCoordinate

            target: null

            onActiveChanged: {
                if (active) {
                    root.followDrone = false;
                    startCoordinate = map.toCoordinate(centroid.position, false);
                }
            }
            // 根据双指缩放倍率调整 STM32 地图级别。
            onScaleChanged: function (delta) {
                if (!startCoordinate || !startCoordinate.isValid)
                    return;
                map.zoomLevel = Math.max(map.minimumZoomLevel, Math.min(map.maximumZoomLevel,
                                                                        map.zoomLevel + Math.log(
                                                                            delta) / Math.LN2));
                map.alignCoordinateToPoint(startCoordinate, centroid.position);
            }
        }
        WheelHandler {
            id: stmMapWheelHandler

            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            // 根据鼠标滚轮缩放 STM32 地图。
            onWheel: function (event) {
                root.followDrone = false;
                var anchorCoordinate = map.toCoordinate(point.position, false);
                map.zoomLevel = Math.max(map.minimumZoomLevel, Math.min(map.maximumZoomLevel,
                                                                        map.zoomLevel
                                                                        + event.angleDelta.y
                                                                        / 120));
                if (anchorCoordinate && anchorCoordinate.isValid)
                    map.alignCoordinateToPoint(anchorCoordinate, point.position);
            }
        }
        MapPolyline {
            id: route

            line.color: "#6b3515bb"
            line.width: 7
        }
        MapPolyline {
            line.color: "#ffad3d"
            line.width: 3
            path: root.trajectory
        }
        MapPolyline {
            line.color: "#57d9ad"
            line.width: 2
            path: [root.currentCoordinate, root.rthCoordinate]
            visible: root.hasPosition && root.hasRthPosition
        }
        MapQuickItem {
            anchorPoint.x: 21
            anchorPoint.y: 21
            coordinate: root.currentCoordinate
            visible: root.hasPosition

            sourceItem: Rectangle {
                border.color: "white"
                border.width: 2
                color: "#ff9f2f"
                height: 42
                radius: 21
                rotation: sensorBackend.heading
                width: 42

                Text {
                    anchors.centerIn: parent
                    color: "#152029"
                    font.bold: true
                    font.pixelSize: 18
                    text: "▲"
                }
            }
        }
        MapQuickItem {
            anchorPoint.x: 16
            anchorPoint.y: 16
            coordinate: root.rthCoordinate
            visible: root.hasRthPosition

            sourceItem: Rectangle {
                border.color: "white"
                border.width: 2
                color: "#2a9d78"
                height: 32
                radius: 16
                width: 32

                Text {
                    anchors.centerIn: parent
                    color: "white"
                    font.bold: true
                    text: "H"
                }
            }
        }
    }
    Column {
        anchors.margins: 12
        anchors.right: parent.right
        anchors.top: mapHeader.bottom
        spacing: 4
        z: 7

        Button {
            enabled: map.zoomLevel < map.maximumZoomLevel
            font.bold: true
            font.pixelSize: 20
            height: 36
            text: "+"
            width: 38

            onClicked: {
                root.followDrone = false;
                map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1);
            }
        }
        Rectangle {
            border.color: "#47616f"
            color: "#101820dd"
            height: 24
            radius: 4
            width: 38

            Text {
                anchors.centerIn: parent
                color: "#dce9ee"
                font.bold: true
                font.pixelSize: 11
                text: Math.round(map.zoomLevel)
            }
        }
        Button {
            enabled: map.zoomLevel > map.minimumZoomLevel
            font.bold: true
            font.pixelSize: 20
            height: 36
            text: "−"
            width: 38

            onClicked: {
                root.followDrone = false;
                map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1);
            }
        }
    }
    Row {
        anchors.bottom: parent.bottom
        anchors.margins: 12
        anchors.right: parent.right
        spacing: 8
        z: 6

        Button {
            text: root.followDrone ? "跟随中" : "重新跟随"

            onClicked: {
                root.followDrone = !root.followDrone;
                if (root.followDrone && root.hasPosition)
                    map.center = root.currentCoordinate;
            }
        }
        Button {
            text: "清除航线"

            onClicked: {
                route.path = [];
                root.trajectory = [];
            }
        }
    }
}
