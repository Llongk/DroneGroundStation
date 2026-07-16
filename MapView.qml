import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 6.5
import QtPositioning 6.5

Rectangle {
    id: root

    property var currentCoordinate: QtPositioning.coordinate(39.9042, 116.4074)
    property bool demoActive: false
    property int demoIndex: 0
    readonly property var demoRoute: [QtPositioning.coordinate(39.9042, 116.4074),
        QtPositioning.coordinate(39.9047, 116.4080), QtPositioning.coordinate(39.9053, 116.4088),
        QtPositioning.coordinate(39.9058, 116.4097), QtPositioning.coordinate(39.9054, 116.4105),
        QtPositioning.coordinate(39.9048, 116.4100), QtPositioning.coordinate(39.9042, 116.4091),
        QtPositioning.coordinate(39.9038, 116.4082)]
    property bool followDrone: true
    property bool hasExternalGps: false
    property bool hasSystemGps: false
    property real markerHeading: 0
    property string positionSourceText: "正在自动定位"
    property var trajectory: []

    // 应用有效坐标，更新标记、地图跟随状态并按需追加轨迹点。
    function applyPosition(coordinate, sourceText, appendToTrack) {
        if (!coordinate || !coordinate.isValid || !isValidCoordinate(coordinate.latitude,
                                                                     coordinate.longitude)) {
            return;
        }

        var previousCoordinate = currentCoordinate;
        var movedDistance = previousCoordinate && previousCoordinate.isValid
                ? previousCoordinate.distanceTo(coordinate) : 0;

        if (movedDistance >= 3) {
            markerHeading = previousCoordinate.azimuthTo(coordinate);
        }

        currentCoordinate = coordinate;
        positionSourceText = sourceText;

        if (followDrone) {
            map.center = coordinate;
        }

        if (appendToTrack) {
            var points = trajectory.slice(0);
            var shouldAppend = points.length === 0;
            if (!shouldAppend) {
                shouldAppend = points[points.length - 1].distanceTo(coordinate) >= 3;
            }

            if (!shouldAppend) {
                return;
            }

            points.push(coordinate);
            if (points.length > 500) {
                points.shift();
            }
            trajectory = points;
        }
    }
    // 判断坐标是否有限、在合法范围内且不为零点。
    function isValidCoordinate(latitude, longitude) {
        return isFinite(latitude) && isFinite(longitude) && latitude >= -90 && latitude <= 90
                && longitude >= -180 && longitude <= 180 && !(Math.abs(latitude) < 0.000001
                                                              && Math.abs(longitude) < 0.000001);
    }
    // 在没有真实 GPS 时启动本地演示轨迹。
    function startDemo() {
        if (hasExternalGps) {
            return;
        }

        demoActive = true;
        demoIndex = 0;
        trajectory = [];
        applyPosition(demoRoute[0], "内置演示轨迹", true);
        demoTimer.start();
    }
    // 停止演示轨迹计时器并恢复真实数据优先级。
    function stopDemo() {
        demoTimer.stop();
        demoActive = false;
        positionSourceText = "正在获取系统定位";
    }

    clip: true
    color: "#1e2630"
    radius: 8

    PositionSource {
        id: systemPosition

        active: !root.hasExternalGps && !root.demoActive
        preferredPositioningMethods: PositionSource.AllPositioningMethods
        updateInterval: 1000

        onPositionChanged: {
            var coordinate = position.coordinate;
            if (root.isValidCoordinate(coordinate.latitude, coordinate.longitude)) {
                root.hasSystemGps = true;
                fallbackTimer.stop();
                root.applyPosition(coordinate, "系统自动定位", true);
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
                root.startDemo();
            }
        }
    }
    Timer {
        id: demoTimer

        interval: 1000
        repeat: true

        onTriggered: {
            root.demoIndex = (root.demoIndex + 1) % root.demoRoute.length;
            root.applyPosition(root.demoRoute[root.demoIndex], "内置演示轨迹", true);
        }
    }
    Connections {
        // 接收 C++ 手机 GPS 信号并更新地图位置。
        function onGpsChanged() {
            if (!root.isValidCoordinate(backend.latitude, backend.longitude)) {
                return;
            }

            var firstExternalPosition = !root.hasExternalGps;
            root.hasExternalGps = true;
            root.hasSystemGps = false;
            demoTimer.stop();
            root.demoActive = false;
            fallbackTimer.stop();

            var coordinate = QtPositioning.coordinate(backend.latitude, backend.longitude);

            if (firstExternalPosition) {
                // 切换到真实 GPS 时丢弃系统定位或演示轨迹，避免跨区域连线。
                root.trajectory = [];
                root.currentCoordinate = coordinate;
                root.markerHeading = 0;
                map.zoomLevel = 18;
            }

            var accuracyText = backend.accuracy > 0 ? " ±" + backend.accuracy.toFixed(0) + "米" : "";
            root.applyPosition(coordinate, "无人机实时 GPS" + accuracyText, true);
        }

        target: backend
    }
    Rectangle {
        id: titleBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#18212b"
        height: 44
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Text {
                color: "#c9a84c"
                font.bold: true
                font.pixelSize: 18
                text: "无人机 GPS 地图"
            }
            Item {
                Layout.fillWidth: true
            }
            Rectangle {
                color: root.hasExternalGps ? "#1f6f50" : root.demoActive ? "#725b18" : "#24546f"
                implicitHeight: 26
                implicitWidth: sourceLabel.implicitWidth + 20
                radius: 13

                Text {
                    id: sourceLabel

                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: 12
                    text: root.positionSourceText
                }
            }
        }
    }
    Map {
        id: map

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        center: root.currentCoordinate
        maximumZoomLevel: 20
        minimumZoomLevel: 3
        zoomLevel: 18

        plugin: Plugin {
            name: "osm"
        }

        // 鼠标拖动平移；手动浏览时暂停自动跟随，避免新GPS帧把地图拉回。
        DragHandler {
            id: mapDragHandler

            target: null

            onActiveChanged: {
                if (active)
                    root.followDrone = false;
            }
            // 将拖动手势位移转换为地图平移。
            onTranslationChanged: function (delta) {
                map.pan(-delta.x, -delta.y);
            }
        }

        // 触摸屏/触摸板双指缩放，缩放中心保持在双指中心位置。
        PinchHandler {
            id: mapPinchHandler

            property var startCoordinate

            target: null

            onActiveChanged: {
                if (active) {
                    root.followDrone = false;
                    startCoordinate = map.toCoordinate(centroid.position, false);
                }
            }
            // 根据双指缩放倍率调整地图级别。
            onScaleChanged: function (delta) {
                if (!startCoordinate || !startCoordinate.isValid)
                    return;
                map.zoomLevel = Math.max(map.minimumZoomLevel, Math.min(map.maximumZoomLevel,
                                                                        map.zoomLevel + Math.log(
                                                                            delta) / Math.LN2));
                map.alignCoordinateToPoint(startCoordinate, centroid.position);
            }
        }

        // 滚轮围绕鼠标所在位置缩放，查看局部航线时不会丢失目标点。
        WheelHandler {
            id: mapWheelHandler

            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            // 根据鼠标滚轮缩放手机地图。
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
        MapQuickItem {
            id: drone

            anchorPoint.x: 18
            anchorPoint.y: 18
            coordinate: root.currentCoordinate

            sourceItem: Rectangle {
                border.color: "white"
                border.width: 2
                color: "#e53935"
                height: 36
                radius: 18
                width: 36

                Text {
                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: 16
                    // 只在实际位移超过静止阈值后更新航向，避免罗盘原地抖动。
                    rotation: root.markerHeading
                    text: "▲"
                }
            }
        }
        MapPolyline {
            line.color: "#8a4f12aa"
            line.width: 7
            path: root.trajectory
        }
        MapPolyline {
            line.color: "#ffd54f"
            line.width: 3
            path: root.trajectory
        }
        MapCircle {
            border.color: "#66aaff99"
            border.width: 1
            center: root.currentCoordinate
            color: "#3388ff22"
            radius: Math.max(backend.accuracy, 3)
            visible: root.hasExternalGps && backend.accuracy > 0
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.margins: 10
        anchors.top: titleBar.bottom
        color: "#101820cc"
        height: 30
        radius: 6
        width: coordinateText.implicitWidth + 18
        z: 10

        Text {
            id: coordinateText

            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 12
            text: root.currentCoordinate.latitude.toFixed(6) + ", "
                  + root.currentCoordinate.longitude.toFixed(6)
        }
    }
    Column {
        anchors.margins: 10
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        spacing: 4
        z: 11

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
        anchors.margins: 10
        anchors.right: parent.right
        spacing: 8
        z: 10

        Button {
            text: root.followDrone ? "跟随中" : "重新跟随"

            onClicked: {
                root.followDrone = !root.followDrone;
                if (root.followDrone) {
                    map.center = root.currentCoordinate;
                }
            }
        }
        Button {
            text: root.demoActive ? "尝试系统定位" : "演示轨迹"
            visible: !root.hasExternalGps

            onClicked: {
                if (root.demoActive) {
                    root.stopDemo();
                } else {
                    fallbackTimer.stop();
                    root.startDemo();
                }
            }
        }
        Button {
            text: "清除轨迹"

            onClicked: root.trajectory = []
        }
    }
}
