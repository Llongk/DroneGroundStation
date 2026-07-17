import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 应用程序主窗口，包含 StackView 页面栈，管理登录页、主页、历史页、STM32 页的导航。
ApplicationWindow {
    id: root

    color: "#1e1e2e" // 深色背景
    height: 720 // 窗口高度
    title: "GCS 地面控制站" // 窗口标题
    visible: true // 窗口可见
    width: 1280 // 窗口宽度

    // 页面栈，管理所有页面的入栈/出栈导航。
    StackView {
        id: stackView

        anchors.fill: parent // 填满父窗口
        initialItem: loginPage // 初始页面为登录页
    }
    // 登录页面组件
    Component {
        id: loginPage

        LoginPage {
            // 登录成功后启动历史会话并进入手机主页。
            onLoginSuccess: function (username) {
                historyDatabase.startSession(username); // 启动飞行历史会话

                stackView.push(mainPage); // 进入主页
            }
        }
    }
    // 主页面组件
    Component {
        id: mainPage

        MainPage {
            // 打开历史页面
            onOpenHistoryRequested: {
                stackView.push(historyPage); // 入栈历史页
            }
            // 打开 STM32 页面
            onOpenStm32Requested: {
                stackView.push(stm32Page); // 入栈 STM32 页
            }
        }
    }
    // 历史记录页面组件
    Component {
        id: historyPage

        HistoryPage {
            onBackRequested: stackView.pop() // 返回上一页
        }
    }
    // STM32 遥测页面组件
    Component {
        id: stm32Page

        Stm32Page {
            onBackRequested: {
                stackView.pop(); // 返回上一页
            }
        }
    }
}