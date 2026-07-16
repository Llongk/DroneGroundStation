#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QDir>
#include <QFile>

#include "backend.h"
#include "historydatabase.h"
#include "sensorbackend.h"

//! 创建后端对象、注册 QML 上下文并启动 Qt 事件循环。
int main(int argc, char *argv[])
{
    // The Windows native style does not allow the customized QML
    // background/contentItem delegates used by this application.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);

    Backend backend;
    SensorBackend sensorBackend;
    HistoryDatabase historyDatabase(&backend, &sensorBackend);

    // These objects live on main()'s stack and must never be adopted or
    // garbage-collected by QML while StackView pages are created/destroyed.
    QQmlEngine::setObjectOwnership(&backend, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&sensorBackend, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&historyDatabase, QQmlEngine::CppOwnership);

    // 在手机活动租约变化时切换 STM32 检测状态。
    QObject::connect(&backend,
                     &Backend::sensorDataReceived,
                     &sensorBackend,
                     &SensorBackend::updateSensorData);
    QObject::connect(&backend,
                     &Backend::phoneConnectionChanged,
                     &sensorBackend,
                     [&backend, &sensorBackend]() {
        sensorBackend.setPhoneDeviceActive(backend.phoneConnected());
    });

    QQmlApplicationEngine engine;
    QStringList startupWarnings;
    // 收集 QML 启动告警，加载失败时写入诊断日志。
    QObject::connect(&engine,
                     &QQmlApplicationEngine::warnings,
                     &app,
                     [&startupWarnings](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            startupWarnings.append(warning.toString());
    });
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("sensorBackend", &sensorBackend);
    engine.rootContext()->setContextProperty("historyDatabase", &historyDatabase);

    engine.loadFromModule("DroneGroundStation", "Main");
    if (engine.rootObjects().isEmpty()) {
        QFile diagnostics(QDir::temp().filePath(
            QStringLiteral("DroneGroundStation-qml-startup.log")));
        if (diagnostics.open(QIODevice::WriteOnly | QIODevice::Text)) {
            diagnostics.write(startupWarnings.join(QLatin1Char('\n')).toUtf8());
        }
        return -1;
    }

    return app.exec();
}
