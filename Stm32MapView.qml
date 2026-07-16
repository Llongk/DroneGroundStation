import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 6.5
import QtPositioning 6.5

Rectangle {
    id: root
    color: "#101a24"
    radius: 12
    clip: true

    property var currentCoordinate: QtPositioning.coordinate(39.9042, 116.4074)
    property var rthCoordinate: QtPositioning.coordinate(0, 0)
    property var trajectory: []
    property bool hasPosition: false
    property bool hasRthPosition: false
    property bool followDrone: true
    property int lastTelemetrySequence: -1

    function validCoordinate(lat, lng) {
        return isFinite(lat) && isFinite(lng)
                && lat >= -90 && lat <= 90
                && lng >= -180 && lng <= 180
                && !(Math.abs(lat) < 0.000001 && Math.abs(lng) < 0.000001)
    }

    function updatePosition() {
        if (!validCoordinate(sensorBackend.stm32Latitude,
                             sensorBackend.stm32Longitude)) {
            return
        }

        var coordinate = QtPositioning.coordinate(sensorBackend.stm32Latitude,
                                                   sensorBackend.stm32Longitude)
        if (!hasPosition) {
            trajectory = []
            map.zoomLevel = 18
        }
        hasPosition = true
        currentCoordinate = coordinate

        var points = trajectory.slice(0)
        var shouldAppend = points.length === 0
        if (!shouldAppend) {
            var distance = points[points.length - 1].distanceTo(coordinate)
            // STM32 telemetry is emitted with six decimal places.  Keep every
            // real coordinate change instead of waiting for a two-metre jump.
            shouldAppend = isFinite(distance) && distance >= 0.05
        }
        if (shouldAppend) {
            points.push(coordinate)
            route.addCoordinate(coordinate)
            if (points.length > 1000) {
                points.shift()
                route.removeCoordinate(0)
            }
            trajectory = points
        }
        lastTelemetrySequence = sensorBackend.sequence

        if (validCoordinate(sensorBackend.rthLatitude,
                            sensorBackend.rthLongitude)) {
            hasRthPosition = true
            rthCoordinate = QtPositioning.coordinate(sensorBackend.rthLatitude,
                                                      sensorBackend.rthLongitude)
        } else {
            hasRthPosition = false
        }

        if (followDrone) map.center = coordinate
    }

    Connections {
        target: sensorBackend
        function onTelemetryChanged() { root.updatePosition() }
    }

    Component.onCompleted: {
        if (sensorBackend.telemetryValid) updatePosition()
    }

    Rectangle {
        id: mapHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 48
        color: "#14232e"
        z: 5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14

            Text {
                text: "STM32 实时航线"
                color: "#f0c65b"
                font.pixelSize: 17
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.hasPosition
                      ? root.currentCoordinate.latitude.toFixed(6) + ", "
                        + root.currentCoordinate.longitude.toFixed(6)
                      : "等待有效坐标"
                color: root.hasPosition ? "#dce9ee" : "#748b99"
                font.pixelSize: 12
            }
        }
    }

    Map {
        id: map
        anchors.top: mapHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        center: root.currentCoordinate
        zoomLevel: 18
        minimumZoomLevel: 3
        maximumZoomLevel: 20

        plugin: Plugin { name: "osm" }

        DragHandler {
            id: stmMapDragHandler
            target: null
            onActiveChanged: {
                if (active)
                    root.followDrone = false
            }
            onTranslationChanged: function(delta) {
                map.pan(-delta.x, -delta.y)
            }
        }

        PinchHandler {
            id: stmMapPinchHandler
            target: null
            property var startCoordinate

            onActiveChanged: {
                if (active) {
                    root.followDrone = false
                    startCoordinate = map.toCoordinate(centroid.position, false)
                }
            }
            onScaleChanged: function(delta) {
                if (!startCoordinate || !startCoordinate.isValid)
                    return
                map.zoomLevel = Math.max(map.minimumZoomLevel,
                                         Math.min(map.maximumZoomLevel,
                                                  map.zoomLevel + Math.log(delta) / Math.LN2))
                map.alignCoordinateToPoint(startCoordinate, centroid.position)
            }
        }

        WheelHandler {
            id: stmMapWheelHandler
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(event) {
                root.followDrone = false
                var anchorCoordinate = map.toCoordinate(point.position, false)
                map.zoomLevel = Math.max(map.minimumZoomLevel,
                                         Math.min(map.maximumZoomLevel,
                                                  map.zoomLevel + event.angleDelta.y / 120))
                if (anchorCoordinate && anchorCoordinate.isValid)
                    map.alignCoordinateToPoint(anchorCoordinate, point.position)
            }
        }

        MapPolyline {
            id: route
            line.width: 7
            line.color: "#6b3515bb"
        }

        MapPolyline {
            path: root.trajectory
            line.width: 3
            line.color: "#ffad3d"
        }

        MapPolyline {
            visible: root.hasPosition && root.hasRthPosition
            path: [root.currentCoordinate, root.rthCoordinate]
            line.width: 2
            line.color: "#57d9ad"
        }

        MapQuickItem {
            visible: root.hasPosition
            coordinate: root.currentCoordinate
            anchorPoint.x: 21
            anchorPoint.y: 21

            sourceItem: Rectangle {
                width: 42
                height: 42
                radius: 21
                color: "#ff9f2f"
                border.color: "white"
                border.width: 2
                rotation: sensorBackend.heading

                Text {
                    anchors.centerIn: parent
                    text: "▲"
                    color: "#152029"
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }

        MapQuickItem {
            visible: root.hasRthPosition
            coordinate: root.rthCoordinate
            anchorPoint.x: 16
            anchorPoint.y: 16

            sourceItem: Rectangle {
                width: 32
                height: 32
                radius: 16
                color: "#2a9d78"
                border.color: "white"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "H"
                    color: "white"
                    font.bold: true
                }
            }
        }
    }

    Column {
        anchors.right: parent.right
        anchors.top: mapHeader.bottom
        anchors.margins: 12
        spacing: 4
        z: 7

        Button {
            width: 38
            height: 36
            text: "+"
            font.pixelSize: 20
            font.bold: true
            enabled: map.zoomLevel < map.maximumZoomLevel
            onClicked: {
                root.followDrone = false
                map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1)
            }
        }

        Rectangle {
            width: 38
            height: 24
            radius: 4
            color: "#101820dd"
            border.color: "#47616f"

            Text {
                anchors.centerIn: parent
                text: Math.round(map.zoomLevel)
                color: "#dce9ee"
                font.pixelSize: 11
                font.bold: true
            }
        }

        Button {
            width: 38
            height: 36
            text: "−"
            font.pixelSize: 20
            font.bold: true
            enabled: map.zoomLevel > map.minimumZoomLevel
            onClicked: {
                root.followDrone = false
                map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1)
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        spacing: 8
        z: 6

        Button {
            text: root.followDrone ? "跟随中" : "重新跟随"
            onClicked: {
                root.followDrone = !root.followDrone
                if (root.followDrone && root.hasPosition)
                    map.center = root.currentCoordinate
            }
        }

        Button {
            text: "清除航线"
            onClicked: {
                route.path = []
                root.trajectory = []
            }
        }
    }
}
