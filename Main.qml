import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root

    color: "#1e1e2e"
    height: 720
    title: "GCS 地面控制站"
    visible: true
    width: 1280

    StackView {
        id: stackView

        anchors.fill: parent
        initialItem: loginPage
    }
    Component {
        id: loginPage

        LoginPage {
            // 登录成功后启动历史会话并进入手机主页。
            onLoginSuccess: function (username) {
                historyDatabase.startSession(username);

                stackView.push(mainPage);
            }
        }
    }
    Component {
        id: mainPage

        MainPage {
            onOpenHistoryRequested: {
                stackView.push(historyPage);
            }
            onOpenStm32Requested: {
                stackView.push(stm32Page);
            }
        }
    }
    Component {
        id: historyPage

        HistoryPage {
            onBackRequested: stackView.pop()
        }
    }
    Component {
        id: stm32Page

        Stm32Page {
            onBackRequested: {
                stackView.pop();
            }
        }
    }
}
