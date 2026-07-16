import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Rectangle {
    id: root

    property bool isLoginMode: true

    signal loginSuccess(string username)

    //==================
    // 登录
    //==================

    // 校验演示账号并通知页面栈进入主页面。
    function login() {
        var user = userInput.text.trim();

        var pwd = passwordInput.text.trim();

        if (user === "admin" && pwd === "123456") {
            console.log("登录成功");

            root.loginSuccess(user);
        } else {
            console.log("用户名或密码错误");
        }
    }

    //==================
    // 注册
    //==================

    // 校验注册表单并返回登录模式；当前不持久化账号。
    function registerUser() {
        var user = userInput.text.trim();

        var pwd = passwordInput.text.trim();

        var confirm = confirmInput.text.trim();

        if (user === "" || pwd === "" || confirm === "") {
            console.log("请输入完整信息");

            return;
        }

        if (pwd !== confirm) {
            console.log("密码不一致");

            return;
        }

        console.log("注册成功");

        isLoginMode = true;

        userInput.text = user;

        passwordInput.clear();

        confirmInput.clear();
    }

    color: "#1a1a2e"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: 360

        Text {
            Layout.alignment: Qt.AlignHCenter
            color: "#c9a84c"
            font.bold: true
            font.pixelSize: 32
            text: "GCS 地面控制站"
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: "#8a9ba8"
            font.pixelSize: 18
            text: isLoginMode ? "用户登录" : "用户注册"
        }
        TextField {
            id: userInput

            Layout.fillWidth: true
            color: "white"
            font.pixelSize: 16
            height: 45
            placeholderText: "用户名"

            background: Rectangle {
                border.color: userInput.activeFocus ? "#c9a84c" : "#444444"
                color: "#2a2a3e"
                radius: 6
            }
        }
        TextField {
            id: passwordInput

            Layout.fillWidth: true
            color: "white"
            echoMode: TextField.Password
            font.pixelSize: 16
            height: 45
            placeholderText: "密码"

            background: Rectangle {
                border.color: passwordInput.activeFocus ? "#c9a84c" : "#444444"
                color: "#2a2a3e"
                radius: 6
            }
        }
        TextField {
            id: confirmInput

            Layout.fillWidth: true
            color: "white"
            echoMode: TextField.Password
            height: 45
            placeholderText: "确认密码"
            visible: !isLoginMode

            background: Rectangle {
                border.color: "#444444"
                color: "#2a2a3e"
                radius: 6
            }
        }
        Button {
            Layout.fillWidth: true
            height: 48
            text: isLoginMode ? "登 录" : "注 册"

            background: Rectangle {
                color: "#c9a84c"
                radius: 6
            }
            contentItem: Text {
                color: "#1a1a2e"
                font.bold: true
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                text: parent.text
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                if (isLoginMode) {
                    login();
                } else {
                    registerUser();
                }
            }
        }
        Button {
            Layout.fillWidth: true
            text: isLoginMode ? "没有账号？注册" : "已有账号？登录"

            background: Rectangle {
                color: "transparent"
            }
            contentItem: Text {
                color: "#8a9ba8"
                horizontalAlignment: Text.AlignHCenter
                text: parent.text
            }

            onClicked: {
                isLoginMode = !isLoginMode;

                userInput.clear();

                passwordInput.clear();

                confirmInput.clear();
            }
        }
    }
}
