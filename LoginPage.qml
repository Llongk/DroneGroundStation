import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

// 登录/注册页面，支持演示账号校验和模式切换。
Rectangle {
    id: root

    // 当前是否为登录模式（false 表示注册模式）
    property bool isLoginMode: true

    // 登录成功信号，携带用户名
    signal loginSuccess(string username)

    //==================
    // 登录
    //==================

    // 校验演示账号并通知页面栈进入主页面。
    function login() {
        var user = userInput.text.trim(); // 获取用户名

        var pwd = passwordInput.text.trim(); // 获取密码

        if (user === "admin" && pwd === "123456") { // 验证演示账号
            console.log("登录成功"); // 登录成功日志

            root.loginSuccess(user); // 发出登录成功信号
        } else {
            console.log("用户名或密码错误"); // 登录失败日志
        }
    }

    //==================
    // 注册
    //==================

    // 校验注册表单并返回登录模式；当前不持久化账号。
    function registerUser() {
        var user = userInput.text.trim(); // 获取用户名

        var pwd = passwordInput.text.trim(); // 获取密码

        var confirm = confirmInput.text.trim(); // 获取确认密码

        if (user === "" || pwd === "" || confirm === "") { // 检查空字段
            console.log("请输入完整信息"); // 提示信息不完整

            return;
        }

        if (pwd !== confirm) { // 检查密码一致性
            console.log("密码不一致"); // 提示密码不一致

            return;
        }

        console.log("注册成功"); // 注册成功日志

        isLoginMode = true; // 切换回登录模式

        userInput.text = user; // 保留用户名便于登录

        passwordInput.clear(); // 清空密码

        confirmInput.clear(); // 清空确认密码
    }

    color: "#1a1a2e" // 深色背景

    // 居中布局
    ColumnLayout {
        anchors.centerIn: parent // 居中
        spacing: 20 // 元素间距
        width: 360 // 固定宽度

        // 应用标题
        Text {
            Layout.alignment: Qt.AlignHCenter // 水平居中
            color: "#c9a84c" // 金色
            font.bold: true // 加粗
            font.pixelSize: 32 // 字号
            text: "GCS 地面控制站" // 标题文本
        }
        // 模式提示（登录/注册）
        Text {
            Layout.alignment: Qt.AlignHCenter // 水平居中
            color: "#8a9ba8" // 灰色
            font.pixelSize: 18 // 字号
            text: isLoginMode ? "用户登录" : "用户注册" // 根据模式显示
        }
        // 用户名输入框
        TextField {
            id: userInput

            Layout.fillWidth: true // 填满宽度
            color: "white" // 白色文字
            font.pixelSize: 16 // 字号
            height: 45 // 输入框高度
            placeholderText: "用户名" // 占位提示

            background: Rectangle {
                border.color: userInput.activeFocus ? "#c9a84c" : "#444444" // 聚焦时金色边框
                color: "#2a2a3e" // 深色背景
                radius: 6 // 圆角
            }
        }
        // 密码输入框
        TextField {
            id: passwordInput

            Layout.fillWidth: true // 填满宽度
            color: "white" // 白色文字
            echoMode: TextField.Password // 密码模式
            font.pixelSize: 16 // 字号
            height: 45 // 输入框高度
            placeholderText: "密码" // 占位提示

            background: Rectangle {
                border.color: passwordInput.activeFocus ? "#c9a84c" : "#444444" // 聚焦时金色边框
                color: "#2a2a3e" // 深色背景
                radius: 6 // 圆角
            }
        }
        // 确认密码输入框（仅注册模式可见）
        TextField {
            id: confirmInput

            Layout.fillWidth: true // 填满宽度
            color: "white" // 白色文字
            echoMode: TextField.Password // 密码模式
            height: 45 // 输入框高度
            placeholderText: "确认密码" // 占位提示
            visible: !isLoginMode // 仅注册模式可见

            background: Rectangle {
                border.color: "#444444" // 深灰边框
                color: "#2a2a3e" // 深色背景
                radius: 6 // 圆角
            }
        }
        // 登录/注册按钮
        Button {
            Layout.fillWidth: true // 填满宽度
            height: 48 // 按钮高度
            text: isLoginMode ? "登 录" : "注 册" // 根据模式切换文本

            background: Rectangle {
                color: "#c9a84c" // 金色背景
                radius: 6 // 圆角
            }
            contentItem: Text {
                color: "#1a1a2e" // 深色文字
                font.bold: true // 加粗
                font.pixelSize: 18 // 字号
                horizontalAlignment: Text.AlignHCenter // 水平居中
                text: parent.text // 继承父文本
                verticalAlignment: Text.AlignVCenter // 垂直居中
            }

            onClicked: {
                if (isLoginMode) {
                    login(); // 登录模式调用登录
                } else {
                    registerUser(); // 注册模式调用注册
                }
            }
        }
        // 模式切换按钮
        Button {
            Layout.fillWidth: true // 填满宽度
            text: isLoginMode ? "没有账号？注册" : "已有账号？登录" // 根据模式切换文本

            background: Rectangle {
                color: "transparent" // 透明背景
            }
            contentItem: Text {
                color: "#8a9ba8" // 灰色文字
                horizontalAlignment: Text.AlignHCenter // 水平居中
                text: parent.text // 继承父文本
            }

            onClicked: {
                isLoginMode = !isLoginMode; // 切换模式

                userInput.clear(); // 清空用户名

                passwordInput.clear(); // 清空密码

                confirmInput.clear(); // 清空确认密码
            }
        }
    }
}