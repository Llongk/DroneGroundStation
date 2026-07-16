#include "sensorbackend.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTimer>
#include <QtMath>

#include <cmath>

namespace {
constexpr quint16 kDefaultTelemetryPort = 8080;
constexpr qsizetype kMaximumBufferedBytes = 64 * 1024;
constexpr int kRetryIntervalMs = 2000;
constexpr int kConnectTimeoutMs = 3500;

bool usableIpv4(const QHostAddress &address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol
        && !address.isNull()
        && !address.isLoopback()
        && !address.toString().startsWith(QStringLiteral("169.254."));
}
}

SensorBackend::SensorBackend(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_retryTimer(new QTimer(this)),
      m_connectTimeout(new QTimer(this)),
      m_telemetryWatchdog(new QTimer(this))
{
    m_targetPort = kDefaultTelemetryPort;
    // Do not reuse command sequence 1 after every desktop restart.  This
    // prevents a delayed ACK retained by the ESP8266 from matching a new run.
    m_commandSequence = static_cast<qulonglong>(
        QDateTime::currentMSecsSinceEpoch() & 0x7fffffffLL);
    if (m_commandSequence == 0)
        m_commandSequence = 1;
    m_socket->setProxy(QNetworkProxy::NoProxy);
    m_retryTimer->setSingleShot(true);
    m_connectTimeout->setSingleShot(true);
    m_telemetryWatchdog->setSingleShot(true);
    m_telemetryWatchdog->setInterval(3500);

    connect(m_retryTimer,
            &QTimer::timeout,
            this,
            &SensorBackend::attemptConnection);
    connect(m_connectTimeout, &QTimer::timeout, this, [this]() {
        if (m_phoneDeviceActive)
            return;
        if (m_socket->state() == QAbstractSocket::ConnectingState) {
            m_socket->abort();
            setConnectionStatus(QStringLiteral("连接超时，正在重试"));
            scheduleReconnect();
        }
    });
    connect(m_telemetryWatchdog, &QTimer::timeout, this, [this]() {
        if (m_phoneDeviceActive)
            return;
        if (m_socket->state() != QAbstractSocket::ConnectedState)
            return;
        qWarning() << "STM32 telemetry watchdog timed out; reconnecting";
        setConnectionStatus(QStringLiteral("遥测超过 3.5 秒未更新，正在自动重连"));
        m_socket->abort();
        scheduleReconnect();
    });
    connect(m_socket,
            &QTcpSocket::connected,
            this,
            &SensorBackend::socketConnected);
    connect(m_socket,
            &QTcpSocket::disconnected,
            this,
            &SensorBackend::socketDisconnected);
    connect(m_socket,
            &QTcpSocket::readyRead,
            this,
            &SensorBackend::readTelemetryData);
    connect(m_socket,
            &QTcpSocket::errorOccurred,
            this,
            &SensorBackend::socketError);

    QTimer::singleShot(0, this, &SensorBackend::attemptConnection);
}

double SensorBackend::temperature() const { return m_temperature; }
double SensorBackend::humidity() const { return m_humidity; }
bool SensorBackend::telemetryConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}
bool SensorBackend::telemetryValid() const { return m_telemetryValid; }
QString SensorBackend::telemetryPeer() const { return m_telemetryPeer; }
QString SensorBackend::telemetryTarget() const
{
    if (m_targetHost.isEmpty()) {
        return QStringLiteral("自动查找热点网关");
    }
    return QStringLiteral("%1:%2").arg(m_targetHost).arg(m_targetPort);
}
QString SensorBackend::telemetryStatus() const { return m_telemetryStatus; }
QString SensorBackend::deviceId() const { return m_deviceId; }
int SensorBackend::protocolVersion() const { return m_protocolVersion; }
qulonglong SensorBackend::sequence() const { return m_sequence; }
double SensorBackend::stm32Latitude() const { return m_stm32Latitude; }
double SensorBackend::stm32Longitude() const { return m_stm32Longitude; }
double SensorBackend::heading() const { return m_heading; }
double SensorBackend::targetAltitude() const { return m_targetAltitude; }
double SensorBackend::stm32Speed() const { return m_stm32Speed; }
double SensorBackend::rthLatitude() const { return m_rthLatitude; }
double SensorBackend::rthLongitude() const { return m_rthLongitude; }
double SensorBackend::rthDistance() const { return m_rthDistance; }
double SensorBackend::dhtTemperature() const { return m_dhtTemperature; }
double SensorBackend::dhtHumidity() const { return m_dhtHumidity; }
double SensorBackend::shtTemperature() const { return m_shtTemperature; }
double SensorBackend::shtHumidity() const { return m_shtHumidity; }
double SensorBackend::mcuTemperature() const { return m_mcuTemperature; }
int SensorBackend::flightMode() const { return m_flightMode; }
int SensorBackend::alarmCode() const { return m_alarmCode; }
int SensorBackend::stm32Battery() const { return m_stm32Battery; }
qulonglong SensorBackend::telemetryTimestamp() const { return m_telemetryTimestamp; }
QString SensorBackend::telemetryUpdateTime() const { return m_telemetryUpdateTime; }
QString SensorBackend::commandStatus() const { return m_commandStatus; }
bool SensorBackend::manualControl() const { return m_manualControl; }
bool SensorBackend::commandPending() const { return m_commandPending; }
qulonglong SensorBackend::commandSequence() const { return m_commandSequence; }

void SensorBackend::setPhoneDeviceActive(bool active)
{
    if (m_phoneDeviceActive == active)
        return;

    m_phoneDeviceActive = active;
    if (active) {
        m_retryTimer->stop();
        m_connectTimeout->stop();
        m_telemetryWatchdog->stop();
        m_buffer.clear();
        m_telemetryPeer.clear();

        if (m_commandPending) {
            m_commandPending = false;
            m_pendingCommandSequence = 0;
            m_commandStatus = QStringLiteral("手机已连接，STM32 控制命令已取消");
            emit commandStateChanged();
        }

        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();

        if (m_telemetryValid) {
            m_telemetryValid = false;
            emit telemetryChanged();
        }
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        qInfo() << "STM32 detection paused while phone device is active";
        return;
    }

    setConnectionStatus(QStringLiteral("手机已断开，恢复 STM32 检测"));
    qInfo() << "Phone device released; resuming STM32 detection";
    QTimer::singleShot(0, this, &SensorBackend::attemptConnection);
}

void SensorBackend::updateSensorData(double temperature, double humidity)
{
    m_temperature = temperature;
    m_humidity = humidity;
    emit sensorChanged();
}

QString SensorBackend::discoverApServerAddress() const
{
    QString fallback;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QString name =
            (interface.name() + QLatin1Char(' ')
             + interface.humanReadableName()).toLower();
        const bool wifi = interface.type() == QNetworkInterface::Wifi
            || name.contains(QStringLiteral("wi-fi"))
            || name.contains(QStringLiteral("wlan"))
            || name.contains(QStringLiteral("无线"));
        if (!wifi) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress localAddress = entry.ip();
            if (!usableIpv4(localAddress)) {
                continue;
            }

            const quint32 local = localAddress.toIPv4Address();
            const quint32 mask = entry.netmask().toIPv4Address();
            if (mask == 0 || mask == 0xffffffffu) {
                continue;
            }

            // AP servers normally occupy the first host address of the WLAN
            // subnet. This derives it from the current hotspot lease instead
            // of fixing a particular 192.168.x.x address in the application.
            const quint32 firstHost = (local & mask) + 1u;
            if (firstHost != local) {
                const QString candidate =
                    QHostAddress(firstHost).toString();
                if (localAddress.toString().startsWith(QStringLiteral("192.168.4."))) {
                    return candidate;
                }
                if (fallback.isEmpty()) {
                    fallback = candidate;
                }
            }
        }
    }
    return fallback;
}

void SensorBackend::attemptConnection()
{
    if (m_phoneDeviceActive) {
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    if (m_automaticTarget) {
        const QString discovered = discoverApServerAddress();
        if (discovered.isEmpty()) {
            m_targetHost.clear();
            setConnectionStatus(QStringLiteral("未发现可用 WLAN 热点，等待网络"));
            scheduleReconnect();
            return;
        }
        m_targetHost = discovered;
        m_targetPort = kDefaultTelemetryPort;
    }

    if (m_targetHost.isEmpty()) {
        setConnectionStatus(QStringLiteral("STM32 服务器地址为空"));
        scheduleReconnect();
        return;
    }

    m_buffer.clear();
    setConnectionStatus(QStringLiteral("正在连接 %1:%2")
                            .arg(m_targetHost)
                            .arg(m_targetPort));
    qInfo() << "Connecting to STM32 telemetry server"
            << m_targetHost << m_targetPort;
    m_socket->connectToHost(m_targetHost, m_targetPort);
    m_connectTimeout->start(kConnectTimeoutMs);
}

void SensorBackend::socketConnected()
{
    if (m_phoneDeviceActive) {
        m_socket->abort();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }

    m_connectTimeout->stop();
    m_retryTimer->stop();
    m_telemetryPeer = m_socket->peerAddress().toString();
    setConnectionStatus(QStringLiteral("已连接，等待遥测数据"));
    m_telemetryWatchdog->start();
    qInfo() << "Connected to STM32 telemetry server:"
            << m_telemetryPeer << m_socket->peerPort();
}

void SensorBackend::socketDisconnected()
{
    m_connectTimeout->stop();
    m_telemetryWatchdog->stop();
    if (m_phoneDeviceActive) {
        m_buffer.clear();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }
    if (m_commandPending) {
        m_commandPending = false;
        m_pendingCommandSequence = 0;
        m_commandStatus = QStringLiteral("STM32 连接断开，待确认命令已取消");
        emit commandStateChanged();
    }
    if (!m_buffer.trimmed().isEmpty()) {
        processTelemetryFrame(m_buffer.trimmed());
    }
    m_buffer.clear();
    setConnectionStatus(QStringLiteral("连接已断开，正在重连"));
    scheduleReconnect();
}

void SensorBackend::socketError()
{
    m_connectTimeout->stop();
    if (m_phoneDeviceActive) {
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }
    const QString error = m_socket->errorString();
    setConnectionStatus(QStringLiteral("连接失败：%1").arg(error));
    qWarning() << "STM32 telemetry connection error:" << error;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    scheduleReconnect();
}

void SensorBackend::scheduleReconnect()
{
    if (m_phoneDeviceActive)
        return;
    if (!m_retryTimer->isActive()) {
        m_retryTimer->start(kRetryIntervalMs);
    }
}

void SensorBackend::setConnectionStatus(const QString &status)
{
    if (m_telemetryStatus == status) {
        return;
    }
    m_telemetryStatus = status;
    emit telemetryConnectionChanged();
}

void SensorBackend::reconnectTelemetry()
{
    m_retryTimer->stop();
    m_connectTimeout->stop();
    m_socket->abort();
    QTimer::singleShot(0, this, &SensorBackend::attemptConnection);
}

void SensorBackend::connectToTelemetry(const QString &host, int port)
{
    const QHostAddress address(host.trimmed());
    if (address.isNull() || port < 1 || port > 65535) {
        setConnectionStatus(QStringLiteral("手动服务器地址或端口无效"));
        return;
    }

    m_automaticTarget = false;
    m_targetHost = address.toString();
    m_targetPort = static_cast<quint16>(port);
    reconnectTelemetry();
}

void SensorBackend::useAutomaticTelemetryTarget()
{
    m_automaticTarget = true;
    reconnectTelemetry();
}

bool SensorBackend::sendFlightCommand(const QString &command,
                                      double headingValue,
                                      double targetAltitudeValue,
                                      double speedValue)
{
    static const QSet<QString> allowedCommands = {
        QStringLiteral("takeoff"),
        QStringLiteral("hover"),
        QStringLiteral("loiter"),
        QStringLiteral("set_heading"),
        QStringLiteral("rth"),
        QStringLiteral("land"),
        QStringLiteral("free_flight")
    };

    const QString normalizedCommand = command.trimmed().toLower();
    if (!allowedCommands.contains(normalizedCommand)) {
        m_commandStatus = QStringLiteral("拒绝未知命令：%1").arg(command);
        emit commandStateChanged();
        return false;
    }
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_commandStatus = QStringLiteral("命令未发送：STM32 TCP 未连接");
        emit commandStateChanged();
        return false;
    }
    if (m_commandPending) {
        m_commandStatus = QStringLiteral("上一条命令仍在等待 STM32 确认");
        emit commandStateChanged();
        return false;
    }
    if (!qIsFinite(headingValue) || !qIsFinite(targetAltitudeValue)
        || !qIsFinite(speedValue)
        || headingValue < 0.0 || headingValue >= 360.0
        || targetAltitudeValue < 0.0 || targetAltitudeValue > 500.0
        || speedValue < 0.0 || speedValue > 50.0) {
        m_commandStatus = QStringLiteral("命令参数越界：航向 0~359°、高度 0~500 m、速度 0~50 m/s");
        emit commandStateChanged();
        return false;
    }

    const bool autonomous = normalizedCommand == QStringLiteral("free_flight");
    ++m_commandSequence;
    m_commandPending = true;
    m_pendingManualControl = !autonomous;
    m_pendingCommandSequence = m_commandSequence;

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("flight_command"));
    object.insert(QStringLiteral("proto"), 1);
    object.insert(QStringLiteral("id"),
                  m_deviceId.isEmpty() ? QStringLiteral("UAV_01") : m_deviceId);
    object.insert(QStringLiteral("command_seq"),
                  static_cast<double>(m_commandSequence));
    object.insert(QStringLiteral("cmd"), normalizedCommand);
    object.insert(QStringLiteral("control"),
                  autonomous ? QStringLiteral("autonomous")
                             : QStringLiteral("manual"));
    object.insert(QStringLiteral("use_preset_route"), autonomous);
    object.insert(QStringLiteral("heading"), headingValue);
    object.insert(QStringLiteral("target_alt"), targetAltitudeValue);
    object.insert(QStringLiteral("speed"), speedValue);
    object.insert(QStringLiteral("ts"),
                  static_cast<double>(QDateTime::currentMSecsSinceEpoch()));

    QByteArray frame = QJsonDocument(object).toJson(QJsonDocument::Compact);
    frame.append("\r\n");
    const qint64 written = m_socket->write(frame);
    if (written != frame.size()) {
        m_commandPending = false;
        m_pendingCommandSequence = 0;
        m_commandStatus = QStringLiteral("命令写入 TCP 失败：%1")
                              .arg(m_socket->errorString());
        emit commandStateChanged();
        return false;
    }
    m_commandStatus = QStringLiteral("已下发 #%1：%2（等待 STM32 确认）")
                          .arg(m_commandSequence)
                          .arg(normalizedCommand);
    qInfo() << "STM32 flight command sent:" << frame.trimmed();
    emit commandStateChanged();

    const qulonglong sentSequence = m_commandSequence;
    QTimer::singleShot(3000, this, [this, sentSequence]() {
        if (m_commandPending && m_pendingCommandSequence == sentSequence) {
            m_commandPending = false;
            m_pendingCommandSequence = 0;
            m_commandStatus = QStringLiteral("命令 #%1 确认超时").arg(sentSequence);
            emit commandStateChanged();
        }
    });
    return true;
}

void SensorBackend::readTelemetryData()
{
    if (m_phoneDeviceActive) {
        m_socket->readAll();
        m_buffer.clear();
        return;
    }

    m_buffer += m_socket->readAll();
    if (m_buffer.size() > kMaximumBufferedBytes) {
        qWarning() << "STM32 telemetry buffer exceeded limit; clearing";
        m_buffer.clear();
        setConnectionStatus(QStringLiteral("接收缓存溢出，已清空"));
        return;
    }

    int newline = -1;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray frame = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (!frame.isEmpty()) {
            processTelemetryFrame(frame);
        }
    }

    if (!m_buffer.isEmpty() && m_buffer.trimmed().endsWith('}')) {
        const QByteArray candidate = m_buffer.trimmed();
        if (processTelemetryFrame(candidate)) {
            m_buffer.clear();
        }
    }
}

bool SensorBackend::finiteNumber(const QJsonObject &object,
                                 const char *key,
                                 double *value)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key));
    if (!jsonValue.isDouble()) {
        return false;
    }
    const double number = jsonValue.toDouble(qQNaN());
    if (!qIsFinite(number)) {
        return false;
    }
    *value = number;
    return true;
}

bool SensorBackend::processTelemetryFrame(const QByteArray &frame)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(frame, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setConnectionStatus(QStringLiteral("收到数据但 JSON 无效：%1")
                                .arg(error.errorString()));
        qWarning() << "Invalid STM32 telemetry JSON:"
                   << error.errorString() << frame.left(160);
        return false;
    }

    const QJsonObject object = document.object();
    const QString frameType = object.value(QStringLiteral("type")).toString();
    if (frameType == QStringLiteral("command_ack")
        || frameType == QStringLiteral("ack")) {
        const QJsonValue sequenceValue = object.value(QStringLiteral("command_seq"));
        const qulonglong ackSequence = sequenceValue.isDouble()
            ? static_cast<qulonglong>(sequenceValue.toDouble()) : 0;
        const bool ok = object.value(QStringLiteral("ok")).toBool(false);
        const QString message = object.value(QStringLiteral("message")).toString();

        if (ackSequence == 0) {
            // A malformed/stale ACK must never cancel the valid command that is
            // currently awaiting confirmation.  Some ESP8266 firmwares can
            // deliver a buffered response from an earlier TCP connection.
            if (!m_commandPending) {
                m_commandStatus = QStringLiteral("已忽略无序命令确认（缺少 command_seq）");
                emit commandStateChanged();
            }
            qWarning() << "STM32 command ACK without a valid sequence:" << frame;
            return true;
        }
        if (m_commandPending && ackSequence != m_pendingCommandSequence) {
            qWarning() << "Ignoring stale STM32 command ACK" << ackSequence
                       << "while waiting for" << m_pendingCommandSequence;
            return true;
        }

        if (ok)
            m_manualControl = m_pendingManualControl;
        m_commandPending = false;
        m_pendingCommandSequence = 0;
        m_commandStatus = ok
            ? QStringLiteral("STM32 已确认命令 #%1%2")
                  .arg(ackSequence)
                  .arg(message.isEmpty() ? QString() : QStringLiteral("：") + message)
            : QStringLiteral("STM32 拒绝命令 #%1%2")
                  .arg(ackSequence)
                  .arg(message.isEmpty() ? QString() : QStringLiteral("：") + message);
        emit commandStateChanged();
        return true;
    }
    const QString id = object.value(QStringLiteral("id")).toString();
    double lat = 0, lng = 0, headingValue = 0, targetAlt = 0;
    double speedValue = 0, rthLat = 0, rthLng = 0, rthDistanceValue = 0;
    double dhtTemp = 0, dhtHum = 0, shtTemp = 0, shtHum = 0, mcuTemp = 0;
    double proto = 0, seq = 0, mode = 0, alarm = 0, battery = 0, timestamp = 0;

    const bool fieldsValid = !id.isEmpty()
        && finiteNumber(object, "proto", &proto)
        && finiteNumber(object, "seq", &seq)
        && finiteNumber(object, "lat", &lat)
        && finiteNumber(object, "lng", &lng)
        && finiteNumber(object, "heading", &headingValue)
        && finiteNumber(object, "target_alt", &targetAlt)
        && finiteNumber(object, "speed", &speedValue)
        && finiteNumber(object, "rth_lat", &rthLat)
        && finiteNumber(object, "rth_lng", &rthLng)
        && finiteNumber(object, "rth_distance", &rthDistanceValue)
        && finiteNumber(object, "dht_temp", &dhtTemp)
        && finiteNumber(object, "dht_hum", &dhtHum)
        && finiteNumber(object, "sht_temp", &shtTemp)
        && finiteNumber(object, "sht_hum", &shtHum)
        && finiteNumber(object, "mcu_temp", &mcuTemp)
        && finiteNumber(object, "mode", &mode)
        && finiteNumber(object, "alarm", &alarm)
        && finiteNumber(object, "battery", &battery)
        && finiteNumber(object, "ts", &timestamp);

    const bool valuesValid = fieldsValid
        && lat >= -90.0 && lat <= 90.0
        && lng >= -180.0 && lng <= 180.0
        && rthLat >= -90.0 && rthLat <= 90.0
        && rthLng >= -180.0 && rthLng <= 180.0
        && speedValue >= 0.0 && rthDistanceValue >= 0.0
        && dhtTemp >= -50.0 && dhtTemp <= 100.0
        && dhtHum >= 0.0 && dhtHum <= 100.0
        && shtTemp >= -50.0 && shtTemp <= 100.0
        && shtHum >= 0.0 && shtHum <= 100.0
        && battery >= 0.0 && battery <= 100.0
        && seq >= 0.0;

    if (!valuesValid) {
        setConnectionStatus(QStringLiteral("JSON 字段缺失或数值越界"));
        qWarning() << "STM32 telemetry frame has missing or invalid fields";
        return false;
    }

    m_deviceId = id;
    m_protocolVersion = qRound(proto);
    m_sequence = static_cast<qulonglong>(seq);
    m_stm32Latitude = lat;
    m_stm32Longitude = lng;
    m_heading = std::fmod(std::fmod(headingValue, 360.0) + 360.0, 360.0);
    m_targetAltitude = targetAlt;
    m_stm32Speed = speedValue;
    m_rthLatitude = rthLat;
    m_rthLongitude = rthLng;
    m_rthDistance = rthDistanceValue;
    m_dhtTemperature = dhtTemp;
    m_dhtHumidity = dhtHum;
    m_shtTemperature = shtTemp;
    m_shtHumidity = shtHum;
    m_mcuTemperature = mcuTemp;
    m_flightMode = qRound(mode);
    m_alarmCode = qRound(alarm);
    m_stm32Battery = qRound(battery);
    m_telemetryTimestamp = static_cast<qulonglong>(timestamp);
    m_telemetryUpdateTime =
        QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    m_telemetryValid = true;
    m_telemetryWatchdog->start();
    m_telemetryPeer = m_socket->peerAddress().toString();
    setConnectionStatus(QStringLiteral("遥测接收正常"));

    if (m_sequence % 10u == 0u) {
        qInfo() << "STM32 telemetry received:"
                << m_deviceId << "seq" << m_sequence
                << "position" << m_stm32Latitude << m_stm32Longitude;
    }
    emit telemetryChanged();
    return true;
}
