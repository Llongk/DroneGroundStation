import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 6.5
import QtPositioning 6.5

Rectangle {
    id: root

    color: "#1e2630"
    radius: 8
    clip: true

    property var trajectory: []
    property bool followDrone: true
    property bool hasExternalGps: false
    property bool hasSystemGps: false
    property bool demoActive: false
    property int demoIndex: 0
    property real markerHeading: 0
    property string positionSourceText: "正在自动定位"
    property var currentCoordinate: QtPositioning.coordinate(39.9042, 116.4074)

    readonly property var demoRoute: [
        QtPositioning.coordinate(39.9042, 116.4074),
        QtPositioning.coordinate(39.9047, 116.4080),
        QtPositioning.coordinate(39.9053, 116.4088),
        QtPositioning.coordinate(39.9058, 116.4097),
        QtPositioning.coordinate(39.9054, 116.4105),
        QtPositioning.coordinate(39.9048, 116.4100),
        QtPositioning.coordinate(39.9042, 116.4091),
        QtPositioning.coordinate(39.9038, 116.4082)
    ]

    function isValidCoordinate(latitude, longitude) {
        return isFinite(latitude)
                && isFinite(longitude)
                && latitude >= -90
                && latitude <= 90
                && longitude >= -180
                && longitude <= 180
                && !(Math.abs(latitude) < 0.000001
                     && Math.abs(longitude) < 0.000001)
    }

    function applyPosition(coordinate, sourceText, appendToTrack) {
        if (!coordinate || !coordinate.isValid
                || !isValidCoordinate(coordinate.latitude,
                                      coordinate.longitude)) {
            return
        }

        var previousCoordinate = currentCoordinate
        var movedDistance = previousCoordinate && previousCoordinate.isValid
                ? previousCoordinate.distanceTo(coordinate)
                : 0

        if (movedDistance >= 3) {
            markerHeading = previousCoordinate.azimuthTo(coordinate)
        }

        currentCoordinate = coordinate
        positionSourceText = sourceText

        if (followDrone) {
            map.center = coordinate
        }

        if (appendToTrack) {
            var points = trajectory.slice(0)
            var shouldAppend = points.length === 0
            if (!shouldAppend) {
                shouldAppend = points[points.length - 1].distanceTo(coordinate) >= 3
            }

            if (!shouldAppend) {
                return
            }

            points.push(coordinate)
            if (points.length > 500) {
                points.shift()
            }
            trajectory = points
        }
    }

    function startDemo() {
        if (hasExternalGps) {
            return
        }

        demoActive = true
        demoIndex = 0
        trajectory = []
        applyPosition(demoRoute[0], "内置演示轨迹", true)
        demoTimer.start()
    }

    function stopDemo() {
        demoTimer.stop()
        demoActive = false
        positionSourceText = "正在获取系统定位"
    }

    PositionSource {
        id: systemPosition
        active: !root.hasExternalGps && !root.demoActive
        updateInterval: 1000
        preferredPositioningMethods: PositionSource.AllPositioningMethods

        onPositionChanged: {
            var coordinate = position.coordinate
            if (root.isValidCoordinate(coordinate.latitude,
                                       coordinate.longitude)) {
                root.hasSystemGps = true
                fallbackTimer.stop()
                root.applyPosition(coordinate, "系统自动定位", true)
            }
        }
    }

    Timer {
        id: fallbackTimer
        interval: 4000
        repeat: false
        running: true

        onTriggered: {
            if (!root.hasExternalGps && !root.hasSystemGps) {
                root.startDemo()
            }
        }
    }

    Timer {
        id: demoTimer
        interval: 1000
        repeat: true

        onTriggered: {
            root.demoIndex = (root.demoIndex + 1) % root.demoRoute.length
            root.applyPosition(root.demoRoute[root.demoIndex],
                               "内置演示轨迹",
                               true)
        }
    }

    Connections {
        target: backend

        function onGpsChanged() {
            if (!root.isValidCoordinate(backend.latitude,
                                        backend.longitude)) {
                return
            }

            var firstExternalPosition = !root.hasExternalGps
            root.hasExternalGps = true
            root.hasSystemGps = false
            demoTimer.stop()
            root.demoActive = false
            fallbackTimer.stop()

            var coordinate = QtPositioning.coordinate(backend.latitude,
                                                       backend.longitude)

            if (firstExternalPosition) {
                // 切换到真实 GPS 时丢弃系统定位或演示轨迹，避免跨区域连线。
                root.trajectory = []
                root.currentCoordinate = coordinate
                root.markerHeading = 0
                map.zoomLevel = 18
            }

            var accuracyText = backend.accuracy > 0
                    ? " ±" + backend.accuracy.toFixed(0) + "米"
                    : ""
            root.applyPosition(coordinate,
                               "无人机实时 GPS" + accuracyText,
                               true)
        }
    }

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 44
        color: "#18212b"
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Text {
                text: "无人机 GPS 地图"
                color: "#c9a84c"
                font.pixelSize: 18
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                implicitWidth: sourceLabel.implicitWidth + 20
                implicitHeight: 26
                radius: 13
                color: root.hasExternalGps ? "#1f6f50"
                                            : root.demoActive ? "#725b18"
                                                              : "#24546f"

                Text {
                    id: sourceLabel
                    anchors.centerIn: parent
                    text: root.positionSourceText
                    color: "white"
                    font.pixelSize: 12
                }
            }
        }
    }

    Map {
        id: map
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        zoomLevel: 18
        minimumZoomLevel: 3
        maximumZoomLevel: 20
        center: root.currentCoordinate

        plugin: Plugin {
            name: "osm"
        }

        // 鼠标拖动平移；手动浏览时暂停自动跟随，避免新GPS帧把地图拉回。
        DragHandler {
            id: mapDragHandler
            target: null
            onActiveChanged: {
                if (active)
                    root.followDrone = false
            }
            onTranslationChanged: function(delta) {
                map.pan(-delta.x, -delta.y)
            }
        }

        // 触摸屏/触摸板双指缩放，缩放中心保持在双指中心位置。
        PinchHandler {
            id: mapPinchHandler
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

        // 滚轮围绕鼠标所在位置缩放，查看局部航线时不会丢失目标点。
        WheelHandler {
            id: mapWheelHandler
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

        MapQuickItem {
            id: drone
            coordinate: root.currentCoordinate
            anchorPoint.x: 18
            anchorPoint.y: 18

            sourceItem: Rectangle {
                width: 36
                height: 36
                radius: 18
                color: "#e53935"
                border.color: "white"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "▲"
                    color: "white"
                    font.pixelSize: 16
                    // 只在实际位移超过静止阈值后更新航向，避免罗盘原地抖动。
                    rotation: root.markerHeading
                }
            }
        }

        MapPolyline {
            line.width: 7
            line.color: "#8a4f12aa"
            path: root.trajectory
        }

        MapPolyline {
            line.width: 3
            line.color: "#ffd54f"
            path: root.trajectory
        }

        MapCircle {
            visible: root.hasExternalGps && backend.accuracy > 0
            center: root.currentCoordinate
            radius: Math.max(backend.accuracy, 3)
            color: "#3388ff22"
            border.color: "#66aaff99"
            border.width: 1
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: titleBar.bottom
        anchors.margins: 10
        width: coordinateText.implicitWidth + 18
        height: 30
        radius: 6
        color: "#101820cc"
        z: 10

        Text {
            id: coordinateText
            anchors.centerIn: parent
            text: root.currentCoordinate.latitude.toFixed(6)
                  + ", "
                  + root.currentCoordinate.longitude.toFixed(6)
            color: "white"
            font.pixelSize: 12
        }
    }

    Column {
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.margins: 10
        spacing: 4
        z: 11

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
        anchors.margins: 10
        spacing: 8
        z: 10

        Button {
            text: root.followDrone ? "跟随中" : "重新跟随"
            onClicked: {
                root.followDrone = !root.followDrone
                if (root.followDrone) {
                    map.center = root.currentCoordinate
                }
            }
        }

        Button {
            visible: !root.hasExternalGps
            text: root.demoActive ? "尝试系统定位" : "演示轨迹"
            onClicked: {
                if (root.demoActive) {
                    root.stopDemo()
                } else {
                    fallbackTimer.stop()
                    root.startDemo()
                }
            }
        }

        Button {
            text: "清除轨迹"
            onClicked: root.trajectory = []
        }
    }
}
