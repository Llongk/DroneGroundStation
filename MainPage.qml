import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// 手机地面站主页面，包含仪表板、数据曲线、姿态仪、告警面板和地图。
Rectangle {
    id: root

    // 手机 GPS 连接状态
    property bool connectionStatus: false
    // 最后一次 GPS 更新时间
    property string lastGpsTime: "--"
    // STM32 连接提示是否已显示
    property bool stm32PromptShown: false
    // 告警消息文本
    property string warningMessage: "等待无人机数据"
    // 是否有活跃告警（用于边框闪动）
    property bool hasActiveAlarm: false

    // 打开历史页面信号
    signal openHistoryRequested
    // 打开 STM32 页面信号
    signal openStm32Requested

    // 根据手机传感器、飞行数据和连接状态生成实时告警。
    function updateWarnings() {
        var messages = []; // 告警消息列表

        if (!connectionStatus) {
            messages.push("GPS 尚未连接"); // 未连接提示
        }
        if (sensorBackend.temperature < -20 || sensorBackend.temperature > 40) {
            messages.push("环境温度超限：" + sensorBackend.temperature.toFixed(1) + " ℃（允许 -20~40 ℃）"); // 温度超限
        }
        if (sensorBackend.humidity > 90) {
            messages.push("湿度异常：" + sensorBackend.humidity.toFixed(1) + " %"); // 湿度异常
        }
        if (Math.abs(backend.roll) > 50) {
            messages.push("横滚角超限：" + backend.roll.toFixed(1) + "°（允许 ±50°）"); // 横滚超限
        }
        if (Math.abs(backend.pitch) > 50) {
            messages.push("俯仰角超限：" + backend.pitch.toFixed(1) + "°（允许 ±50°）"); // 俯仰超限
        }
        if (connectionStatus && backend.battery > 0 && backend.battery < 20) {
            messages.push("电量过低：" + backend.battery.toFixed(0) + " %"); // 电量过低
        }
        if (backend.speed > 50) {
            messages.push("速度超限：" + backend.speed.toFixed(1) + " m/s（上限 50）"); // 速度超限
        }
        if (backend.height < -100 || backend.height > 500) {
            messages.push("高度超限：" + backend.height.toFixed(1) + " m（允许 -100~500 m）"); // 高度超限
        }
        if (backend.accuracy > 50) {
            messages.push("GPS 精度异常：误差 " + backend.accuracy.toFixed(1) + " m（上限 50 m）"); // 精度异常
        }
        // 连接等待只作状态提示；只有真实遥测越过阈值时才启动闪动提醒。
        hasActiveAlarm = (sensorBackend.temperature < -20 || sensorBackend.temperature > 40)
                || sensorBackend.humidity > 90
                || Math.abs(backend.roll) > 50
                || Math.abs(backend.pitch) > 50
                || (connectionStatus && backend.battery > 0 && backend.battery < 20)
                || backend.speed > 50
                || backend.height < -100 || backend.height > 500
                || backend.accuracy > 50; // 汇总活跃告警
        warningMessage = messages.length > 0 ? messages.join("\n") : "系统运行正常"; // 汇总告警文本
    }

    color: "#081018" // 深蓝黑色背景

    // 组件完成时初始化告警检查
    Component.onCompleted: {
        updateWarnings(); // 初始告警计算
        if (sensorBackend.telemetryConnected) { // STM32 已连接
            root.stm32PromptShown = true; // 设置提示已显示
            Qt.callLater(stm32ConnectedPopup.open); // 延迟弹出连接提示
        }
    }

    // 顶部导航栏
    Rectangle {
        id: topBar

        anchors.left: parent.left // 左对齐
        anchors.margins: 14 // 边距
        anchors.right: parent.right // 右对齐
        anchors.top: parent.top // 顶对齐
        border.color: "#263f4e" // 边框颜色
        color: "#101c26" // 背景色
        height: 48 // 高度
        radius: 11 // 圆角

        RowLayout {
            anchors.fill: parent // 填满父容器
            anchors.leftMargin: 12 // 左边距
            anchors.rightMargin: 10 // 右边距
            spacing: 10 // 间距

            // 图标
            Rectangle {
                Layout.preferredHeight: 30 // 高度
                Layout.preferredWidth: 30 // 宽度
                color: "#146b7b" // 青色背景
                radius: 8 // 圆角

                Text {
                    anchors.centerIn: parent // 居中
                    color: "white" // 白色
                    font.bold: true // 加粗
                    font.pixelSize: 15 // 字号
                    text: "▲" // 图标字符
                }
            }
            // 标题
            Text {
                color: "#f4f7fa" // 浅色
                font.bold: true // 加粗
                font.pixelSize: 16 // 字号
                text: "手机地面站" // 标题文本
            }
            // 连接状态指示灯
            Rectangle {
                Layout.preferredHeight: 8 // 高度
                Layout.preferredWidth: 8 // 宽度
                color: root.connectionStatus ? "#42d49b" : "#6d7780" // 连接时绿色
                radius: 4 // 圆角
            }
            // 访问地址
            TextInput {
                Layout.preferredWidth: 210 // 宽度
                color: backend.serverRunning ? "#58d7ed" : "#ff7e86" // 服务运行中青色
                font.bold: true // 加粗
                font.pixelSize: 12 // 字号
                readOnly: true // 只读
                selectByMouse: true // 可选中复制
                selectionColor: "#286273" // 选中颜色
                text: backend.serverRunning ? backend.accessUrl : "手机服务 8080 不可用" // 显示访问地址
                verticalAlignment: Text.AlignVCenter // 垂直居中
            }
            Item {
                Layout.fillWidth: true // 弹性空间
            }
            // 云端设置按钮
            Button {
                id: cloudButton

                Layout.preferredHeight: 34 // 高度
                Layout.preferredWidth: 112 // 宽度
                text: cloudBackend.connected ? "云端 · 在线" : "云端设置" // 根据连接状态切换

                background: Rectangle {
                    border.color: cloudBackend.connected ? "#55dbac" : "#2d687e" // 连接时绿色边框
                    color: cloudBackend.connected ? "#176a53" : "#173848" // 连接时深绿背景
                    radius: 8 // 圆角
                }
                contentItem: Text {
                    color: "#f2fbff" // 浅色文字
                    font.bold: true // 加粗
                    font.pixelSize: 12 // 字号
                    horizontalAlignment: Text.AlignHCenter // 水平居中
                    text: cloudButton.text // 继承按钮文本
                    verticalAlignment: Text.AlignVCenter // 垂直居中
                }

                onClicked: cloudSettingsPopup.open() // 打开云端设置弹窗
            }
            // 历史数据按钮
            Button {
                id: historyButton

                Layout.preferredHeight: 34 // 高度
                Layout.preferredWidth: 100 // 宽度
                text: "历史数据" // 按钮文本

                background: Rectangle {
                    border.color: "#2d687e" // 边框颜色
                    color: historyButton.down ? "#24576d" : "#173848" // 按下时加深
                    radius: 8 // 圆角
                }
                contentItem: Text {
                    color: "#eaf4f8" // 浅色文字
                    font.bold: true // 加粗
                    font.pixelSize: 12 // 字号
                    horizontalAlignment: Text.AlignHCenter // 水平居中
                    text: historyButton.text // 继承按钮文本
                    verticalAlignment: Text.AlignVCenter // 垂直居中
                }

                onClicked: root.openHistoryRequested() // 发出打开历史页面信号
            }
            // STM32 连接状态文本
            Text {
                Layout.maximumWidth: 260 // 最大宽度
                color: sensorBackend.telemetryConnected ? "#65dcae" : "#8397a3" // 连接时绿色
                elide: Text.ElideRight // 溢出省略
                font.pixelSize: 11 // 字号
                text: sensorBackend.telemetryStatus // 连接状态文本
            }
            // STM32 控制按钮
            Button {
                id: stm32Button

                Layout.preferredHeight: 34 // 高度
                Layout.preferredWidth: 120 // 宽度
                text: sensorBackend.telemetryConnected ? "STM32 控制 · 在线" : "STM32 控制" // 根据连接状态切换

                background: Rectangle {
                    border.color: sensorBackend.telemetryConnected ? "#54e0ae" : "#2c6b84" // 连接时绿色边框
                    color: sensorBackend.telemetryConnected ? "#16815f" : "#17465c" // 连接时深绿背景
                    radius: 8 // 圆角
                }
                contentItem: Text {
                    color: "white" // 白色文字
                    font.bold: true // 加粗
                    font.pixelSize: 12 // 字号
                    horizontalAlignment: Text.AlignHCenter // 水平居中
                    text: stm32Button.text // 继承按钮文本
                    verticalAlignment: Text.AlignVCenter // 垂直居中
                }

                onClicked: root.openStm32Requested() // 发出打开 STM32 页面信号
            }
        }
    }
    // 主内容区域（左右布局）
    RowLayout {
        anchors.bottom: parent.bottom // 底对齐
        anchors.bottomMargin: 14 // 底部边距
        anchors.left: parent.left // 左对齐
        anchors.leftMargin: 14 // 左边距
        anchors.right: parent.right // 右对齐
        anchors.rightMargin: 14 // 右边距
        anchors.top: topBar.bottom // 顶对齐导航栏下方
        anchors.topMargin: 12 // 顶部边距
        spacing: 14 // 间距

        // 左侧数据面板
        Rectangle {
            id: dataPanel

            Layout.fillHeight: true // 填满高度
            Layout.maximumWidth: 520 // 最大宽度
            Layout.minimumWidth: 340 // 最小宽度
            Layout.preferredWidth: (root.width - 42) * 0.30 // 30% 宽度
            border.color: "#263b4a" // 边框颜色
            border.width: 1 // 边框宽度
            clip: true // 裁剪溢出
            color: "#101a24" // 背景色
            radius: 14 // 圆角

            ColumnLayout {
                anchors.fill: parent // 填满父容器
                anchors.margins: 14 // 边距
                spacing: 10 // 间距

                // 仪表板
                Rectangle {
                    Layout.fillWidth: true // 填满宽度
                    Layout.maximumHeight: 280 // 最大高度
                    Layout.minimumHeight: 280 // 最小高度
                    Layout.preferredHeight: 280 // 首选高度
                    border.color: "#243947" // 边框颜色
                    clip: true // 裁剪溢出
                    color: "#111f2a" // 背景色
                    radius: 11 // 圆角

                    Dashboard {
                        anchors.fill: parent // 填满父容器
                    }
                }
                // 传感器摘要卡片
                RowLayout {
                    Layout.fillWidth: true // 填满宽度
                    Layout.maximumHeight: 54 // 最大高度
                    Layout.minimumHeight: 54 // 最小高度
                    Layout.preferredHeight: 54 // 首选高度
                    spacing: 8 // 间距

                    // 温度卡片
                    Rectangle {
                        Layout.fillHeight: true // 填满高度
                        Layout.fillWidth: true // 填满宽度
                        color: "#132631" // 背景色
                        radius: 9 // 圆角

                        Column {
                            anchors.centerIn: parent // 居中
                            spacing: 2 // 间距

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: "#ffb56b" // 橙色
                                font.bold: true // 加粗
                                font.pixelSize: 16 // 字号
                                text: sensorBackend.temperature.toFixed(1) + " ℃" // 温度值
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: "#718a9b" // 灰色标签
                                font.pixelSize: 10 // 字号
                                text: "环境温度" // 标签
                            }
                        }
                    }
                    // 湿度卡片
                    Rectangle {
                        Layout.fillHeight: true // 填满高度
                        Layout.fillWidth: true // 填满宽度
                        color: "#132631" // 背景色
                        radius: 9 // 圆角

                        Column {
                            anchors.centerIn: parent // 居中
                            spacing: 2 // 间距

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: "#65c9ff" // 蓝色
                                font.bold: true // 加粗
                                font.pixelSize: 16 // 字号
                                text: sensorBackend.humidity.toFixed(1) + " %" // 湿度值
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: "#718a9b" // 灰色标签
                                font.pixelSize: 10 // 字号
                                text: "环境湿度" // 标签
                            }
                        }
                    }
                    // GPS 更新时间卡片
                    Rectangle {
                        Layout.fillHeight: true // 填满高度
                        Layout.fillWidth: true // 填满宽度
                        color: root.warningMessage === "系统运行正常" ? "#14332c" : "#44282d" // 正常时绿色，异常时红色
                        radius: 9 // 圆角

                        Column {
                            anchors.centerIn: parent // 居中
                            spacing: 2 // 间距

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: root.connectionStatus ? "#73e2b8" : "#a0a9af" // 连接时绿色
                                font.bold: true // 加粗
                                font.pixelSize: 15 // 字号
                                text: root.connectionStatus ? root.lastGpsTime : "--" // GPS 时间
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                                color: "#718a9b" // 灰色标签
                                font.pixelSize: 10 // 字号
                                text: "GPS 更新" // 标签
                            }
                        }
                    }
                }
                // 标签页切换栏
                TabBar {
                    id: dataTabs

                    Layout.fillWidth: true // 填满宽度
                    Layout.maximumHeight: 38 // 最大高度
                    Layout.minimumHeight: 38 // 最小高度
                    Layout.preferredHeight: 38 // 首选高度
                    spacing: 4 // 间距

                    background: Rectangle {
                        color: "#09131c" // 深色背景
                        radius: 9 // 圆角
                    }

                    Repeater {
                        model: ["数据曲线", "姿态仪", "告警"] // 标签列表

                        TabButton {
                            id: tabButton

                            required property string modelData // 标签文本

                            font.bold: checked // 选中时加粗
                            font.pixelSize: 12 // 字号
                            text: modelData // 标签文本

                            background: Rectangle {
                                color: tabButton.checked ? "#173848" : "transparent" // 选中时高亮
                                radius: 7 // 圆角
                            }
                            contentItem: Text {
                                color: tabButton.checked ? "#67d8ee" : "#718899" // 选中时青色
                                font: tabButton.font // 继承字体
                                horizontalAlignment: Text.AlignHCenter // 水平居中
                                text: tabButton.text // 标签文本
                                verticalAlignment: Text.AlignVCenter // 垂直居中
                            }
                        }
                    }
                }
                // 标签页内容区
                StackLayout {
                    Layout.fillHeight: true // 填满高度
                    Layout.fillWidth: true // 填满宽度
                    currentIndex: dataTabs.currentIndex // 绑定当前标签

                    // 数据曲线页
                    Rectangle {
                        clip: true // 裁剪溢出
                        color: "#111f2a" // 背景色
                        radius: 11 // 圆角

                        FlightCurve {
                            anchors.fill: parent // 填满父容器
                        }
                    }
                    // 姿态仪页
                    Rectangle {
                        clip: true // 裁剪溢出
                        color: "#111f2a" // 背景色
                        radius: 11 // 圆角

                        AttitudeIndicator {
                            anchors.fill: parent // 填满父容器
                        }
                    }
                    // 告警页
                    Rectangle {
                        border.color: root.warningMessage === "系统运行正常" ? "#245d49" : "#71414a" // 异常时红色边框
                        color: root.warningMessage === "系统运行正常" ? "#102820" : "#321d22" // 异常时红色背景
                        radius: 11 // 圆角

                        ColumnLayout {
                            anchors.fill: parent // 填满父容器
                            anchors.margins: 16 // 边距
                            spacing: 10 // 间距

                            // 状态标题
                            Text {
                                color: root.warningMessage === "系统运行正常" ? "#70dfad" : "#ff8e96" // 正常时绿色
                                font.bold: true // 加粗
                                font.pixelSize: 17 // 字号
                                text: root.warningMessage === "系统运行正常" ? "✓ 系统运行正常" : "! 需要注意" // 状态文本
                            }
                            // 分隔线
                            Rectangle {
                                Layout.fillWidth: true // 填满宽度
                                Layout.preferredHeight: 1 // 高度
                                color: "#ffffff18" // 半透明白色
                            }
                            // 告警详情
                            Text {
                                Layout.fillHeight: true // 填满高度
                                Layout.fillWidth: true // 填满宽度
                                color: "#dce5eb" // 浅色文字
                                font.pixelSize: 14 // 字号
                                lineHeight: 1.45 // 行高
                                text: root.warningMessage // 告警消息
                                wrapMode: Text.WordWrap // 自动换行
                            }
                        }
                    }
                }
            }
        }
        // 右侧地图面板
        Rectangle {
            id: mapPanel

            Layout.fillHeight: true // 填满高度
            Layout.fillWidth: true // 填满宽度
            Layout.minimumWidth: 560 // 最小宽度
            border.color: "#263b4a" // 边框颜色
            border.width: 1 // 边框宽度
            clip: true // 裁剪溢出
            color: "#101a24" // 背景色
            radius: 14 // 圆角

            MapView {
                anchors.fill: parent // 填满父容器
                anchors.margins: 1 // 边距
            }
        }
    }
    // 页面边缘持续呼吸闪动，使用户未切换到"告警"标签时也能看到异常。
    Rectangle {
        id: alarmFlashFrame

        anchors.fill: parent // 覆盖整个页面
        border.color: "#ff4655" // 红色边框
        border.width: 5 // 边框宽度
        color: "transparent" // 透明填充
        opacity: 1 // 初始透明度
        radius: 3 // 圆角
        visible: root.hasActiveAlarm // 有活跃告警时可见
        z: 100 // 置于顶层

        SequentialAnimation on opacity {
            loops: Animation.Infinite // 无限循环
            running: root.hasActiveAlarm // 有活跃告警时运行

            NumberAnimation {
                duration: 520 // 持续时间
                easing.type: Easing.InOutQuad // 缓动曲线
                from: 0.18 // 最低透明度
                to: 1.0 // 最高透明度
            }
            NumberAnimation {
                duration: 520 // 持续时间
                easing.type: Easing.InOutQuad // 缓动曲线
                from: 1.0 // 最高透明度
                to: 0.18 // 最低透明度
            }
        }
    }
    // 云端设置弹窗
    Popup {
        id: cloudSettingsPopup

        anchors.centerIn: parent // 居中
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside // 按 ESC 或点击外部关闭
        focus: true // 获取焦点
        height: 520 // 高度
        modal: true // 模态
        padding: 0 // 无内边距
        width: 560 // 宽度

        background: Rectangle {
            border.color: cloudBackend.connected ? "#43c99a" : "#315568" // 连接时绿色边框
            border.width: 1 // 边框宽度
            color: "#101d27" // 背景色
            radius: 14 // 圆角
        }
        contentItem: ColumnLayout {
            anchors.fill: parent // 填满父容器
            anchors.margins: 22 // 边距
            spacing: 10 // 间距

            // 标题
            Text {
                color: "#eef7fb" // 浅色
                font.bold: true // 加粗
                font.pixelSize: 22 // 字号
                text: "EMQX 云端上传" // 标题
            }
            // 状态文本
            Text {
                Layout.fillWidth: true // 填满宽度
                color: cloudBackend.connected ? "#63dfae" : "#9fb4bf" // 连接时绿色
                elide: Text.ElideRight // 溢出省略
                font.pixelSize: 12 // 字号
                text: cloudBackend.status + "  ·  待上传 " + cloudBackend.pendingCount + " 条" // 状态和待上传数
            }
            // 连接地址标签
            Text {
                color: "#89a8b8" // 灰色
                font.pixelSize: 12 // 字号
                text: "连接地址" // 标签
            }
            // 连接地址输入框
            TextField {
                id: cloudHostField

                Layout.fillWidth: true // 填满宽度
                selectByMouse: true // 可选中
                text: cloudBackend.host // 当前地址
            }
            // 端口和用户名行
            RowLayout {
                Layout.fillWidth: true // 填满宽度
                spacing: 10 // 间距

                ColumnLayout {
                    Layout.fillWidth: true // 填满宽度
                    spacing: 4 // 间距

                    Text {
                        color: "#89a8b8" // 灰色
                        font.pixelSize: 12 // 字号
                        text: "TLS 端口" // 标签
                    }
                    TextField {
                        id: cloudPortField

                        Layout.fillWidth: true // 填满宽度
                        inputMethodHints: Qt.ImhDigitsOnly // 仅数字输入
                        selectByMouse: true // 可选中
                        text: String(cloudBackend.port) // 当前端口
                        validator: IntValidator { bottom: 1; top: 65535 } // 端口范围验证
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true // 填满宽度
                    spacing: 4 // 间距

                    Text {
                        color: "#89a8b8" // 灰色
                        font.pixelSize: 12 // 字号
                        text: "认证用户名" // 标签
                    }
                    TextField {
                        id: cloudUsernameField

                        Layout.fillWidth: true // 填满宽度
                        selectByMouse: true // 可选中
                        text: cloudBackend.username // 当前用户名
                    }
                }
            }
            // 认证密码标签
            Text {
                color: "#89a8b8" // 灰色
                font.pixelSize: 12 // 字号
                text: "认证密码" // 标签
            }
            // 认证密码输入框
            TextField {
                id: cloudPasswordField

                Layout.fillWidth: true // 填满宽度
                echoMode: TextInput.Password // 密码模式
                placeholderText: "首次启用时输入；留空表示保留已保存密码" // 占位提示
                selectByMouse: true // 可选中
            }
            // 客户端 ID 标签
            Text {
                color: "#89a8b8" // 灰色
                font.pixelSize: 12 // 字号
                text: "客户端 ID" // 标签
            }
            // 客户端 ID 输入框
            TextField {
                id: cloudClientIdField

                Layout.fillWidth: true // 填满宽度
                selectByMouse: true // 可选中
                text: cloudBackend.clientId // 当前客户端 ID
            }
            // 启用复选框
            CheckBox {
                id: cloudEnabledCheck

                checked: cloudBackend.enabled // 当前启用状态
                text: "启用 TLS 云端上传和 SQLite 离线补传" // 标签
            }
            // 主题说明
            Text {
                Layout.fillWidth: true // 填满宽度
                color: "#7895a4" // 灰色
                font.pixelSize: 11 // 字号
                text: "手机：dgs/UAV_01/phone/telemetry\nSTM32：dgs/UAV_01/stm32/telemetry" // 主题说明
            }
            Item {
                Layout.fillHeight: true // 弹性空间
            }
            // 按钮行
            RowLayout {
                Layout.fillWidth: true // 填满宽度
                spacing: 10 // 间距

                Button {
                    text: "立即重连" // 按钮文本
                    onClicked: cloudBackend.reconnect() // 触发重连
                }
                Item {
                    Layout.fillWidth: true // 弹性空间
                }
                Button {
                    text: "取消" // 按钮文本
                    onClicked: cloudSettingsPopup.close() // 关闭弹窗
                }
                Button {
                    text: "保存并应用" // 按钮文本

                    onClicked: {
                        if (cloudBackend.configure(cloudHostField.text, // 主机地址
                                                   Number(cloudPortField.text), // 端口
                                                   cloudUsernameField.text, // 用户名
                                                   cloudPasswordField.text, // 密码
                                                   cloudClientIdField.text, // 客户端 ID
                                                   cloudEnabledCheck.checked)) { // 启用状态
                            cloudPasswordField.clear(); // 清空密码
                            cloudSettingsPopup.close(); // 关闭弹窗
                        }
                    }
                }
            }
        }
    }
    // STM32 已连接提示弹窗
    Popup {
        id: stm32ConnectedPopup

        anchors.centerIn: parent // 居中
        closePolicy: Popup.CloseOnEscape // 按 ESC 关闭
        focus: true // 获取焦点
        height: 230 // 高度
        modal: true // 模态
        padding: 0 // 无内边距
        width: 420 // 宽度

        background: Rectangle {
            border.color: "#38b88c" // 绿色边框
            border.width: 1 // 边框宽度
            color: "#101d27" // 背景色
            radius: 14 // 圆角
        }
        contentItem: ColumnLayout {
            anchors.fill: parent // 填满父容器
            anchors.margins: 22 // 边距
            spacing: 12 // 间距

            // 标题
            Text {
                color: "#64e2b2" // 绿色
                font.bold: true // 加粗
                font.pixelSize: 22 // 字号
                text: "STM32 已连接" // 标题
            }
            // 提示文本
            Text {
                Layout.fillWidth: true // 填满宽度
                color: "#c8d6dd" // 浅色
                font.pixelSize: 14 // 字号
                text: "检测到 " + (sensorBackend.telemetryPeer || "STM32 设备")
                      + " 的遥测服务器已连接。是否进入 STM32 控制页面？" // 提示文本
                wrapMode: Text.WordWrap // 自动换行
            }
            Item {
                Layout.fillHeight: true // 弹性空间
            }
            RowLayout {
                Layout.fillWidth: true // 填满宽度
                spacing: 10 // 间距

                Item {
                    Layout.fillWidth: true // 弹性空间
                }
                Button {
                    text: "稍后" // 按钮文本

                    onClicked: stm32ConnectedPopup.close() // 关闭弹窗
                }
                Button {
                    text: "进入控制页面" // 按钮文本

                    onClicked: {
                        stm32ConnectedPopup.close(); // 关闭弹窗
                        root.openStm32Requested(); // 进入 STM32 页面
                    }
                }
            }
        }
    }
    // 后端连接信号
    Connections {
        // 手机 GPS 更新后刷新连接状态和告警。
        function onGpsChanged() {
            root.connectionStatus = true; // 设置连接状态
            root.lastGpsTime = backend.gpsTime || "--"; // 更新 GPS 时间
            root.updateWarnings(); // 重新计算告警
        }

        target: backend // 绑定 Backend 对象
    }
    Connections {
        // 手机温湿度更新后重新计算告警。
        function onSensorChanged() {
            root.updateWarnings(); // 重新计算告警
        }
        // STM32 连接状态变化时更新入口提示和告警。
        function onTelemetryConnectionChanged() {
            if (sensorBackend.telemetryConnected && !root.stm32PromptShown) { // 首次连接
                root.stm32PromptShown = true; // 设置提示已显示
                stm32ConnectedPopup.open(); // 弹出连接提示
            }
        }

        target: sensorBackend // 绑定 SensorBackend 对象
    }
}