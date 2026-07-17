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

//! 判断网卡地址是否适合用于发现 STM32 AP 网关。
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
    // 连接超时后中止 socket 并安排下一次尝试。
    connect(m_connectTimeout, &QTimer::timeout, this, [this]() {
        if (m_phoneDeviceActive)
            return;
        if (m_socket->state() == QAbstractSocket::ConnectingState) {
            m_socket->abort();
            setConnectionStatus(QStringLiteral("连接超时，正在重试"));
            scheduleReconnect();
        }
    });
    // 遥测长时间未更新时断开并重新建立连接。
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

double SensorBackend::temperature() const { return m_temperature; } // 返回手机上报的环境温度
double SensorBackend::humidity() const { return m_humidity; } // 返回手机上报的环境湿度
bool SensorBackend::telemetryConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState; // 检查 socket 是否已连接
}
bool SensorBackend::telemetryValid() const { return m_telemetryValid; } // 返回是否已收到有效遥测帧
QString SensorBackend::telemetryPeer() const { return m_telemetryPeer; } // 返回当前 STM32 对端地址
QString SensorBackend::telemetryTarget() const
{
    if (m_targetHost.isEmpty()) {
        return QStringLiteral("自动查找热点网关"); // 自动模式提示
    }
    return QStringLiteral("%1:%2").arg(m_targetHost).arg(m_targetPort); // 手动模式显示地址端口
}
QString SensorBackend::telemetryStatus() const { return m_telemetryStatus; } // 返回遥测连接状态文本
QString SensorBackend::deviceId() const { return m_deviceId; } // 返回遥测中的设备 ID
int SensorBackend::protocolVersion() const { return m_protocolVersion; } // 返回遥测协议版本
qulonglong SensorBackend::sequence() const { return m_sequence; } // 返回最新遥测序号
double SensorBackend::stm32Latitude() const { return m_stm32Latitude; } // 返回 STM32 当前纬度
double SensorBackend::stm32Longitude() const { return m_stm32Longitude; } // 返回 STM32 当前经度
double SensorBackend::heading() const { return m_heading; } // 返回归一化后的航向角
double SensorBackend::targetAltitude() const { return m_targetAltitude; } // 返回当前目标高度
double SensorBackend::stm32Speed() const { return m_stm32Speed; } // 返回 STM32 飞行速度
double SensorBackend::rthLatitude() const { return m_rthLatitude; } // 返回返航点纬度
double SensorBackend::rthLongitude() const { return m_rthLongitude; } // 返回返航点经度
double SensorBackend::rthDistance() const { return m_rthDistance; } // 返回当前位置到返航点的距离
double SensorBackend::dhtTemperature() const { return m_dhtTemperature; } // 返回 DHT 温度
double SensorBackend::dhtHumidity() const { return m_dhtHumidity; } // 返回 DHT 湿度
double SensorBackend::shtTemperature() const { return m_shtTemperature; } // 返回 SHT 温度
double SensorBackend::shtHumidity() const { return m_shtHumidity; } // 返回 SHT 湿度
double SensorBackend::mcuTemperature() const { return m_mcuTemperature; } // 返回 MCU 内部温度
int SensorBackend::flightMode() const { return m_flightMode; } // 返回固件飞行模式代码
int SensorBackend::alarmCode() const { return m_alarmCode; } // 返回固件告警代码
int SensorBackend::stm32Battery() const { return m_stm32Battery; } // 返回 STM32 模拟或实测电量
qulonglong SensorBackend::telemetryTimestamp() const { return m_telemetryTimestamp; } // 返回设备侧遥测时间戳
QString SensorBackend::telemetryUpdateTime() const { return m_telemetryUpdateTime; } // 返回本机最近一次遥测更新时间
QString SensorBackend::commandStatus() const { return m_commandStatus; } // 返回最近一次控制命令状态
bool SensorBackend::manualControl() const { return m_manualControl; } // 返回当前是否处于人工控制模式
bool SensorBackend::commandPending() const { return m_commandPending; } // 返回是否有命令正在等待确认
qulonglong SensorBackend::commandSequence() const { return m_commandSequence; } // 返回最近生成的命令序号

// 手机活动时暂停或恢复所有 STM32 检测流程。
// active=true 时中断当前连接，取消待确认命令并停止所有计时器。
void SensorBackend::setPhoneDeviceActive(bool active)
{
    if (m_phoneDeviceActive == active)
        return; // 状态未变化，无需处理

    m_phoneDeviceActive = active; // 更新手机活动状态
    if (active) { // 手机连接时暂停 STM32 检测
        m_retryTimer->stop(); // 停止重连计时器
        m_connectTimeout->stop(); // 停止连接超时计时器
        m_telemetryWatchdog->stop(); // 停止遥测看门狗
        m_buffer.clear(); // 清空接收缓冲区
        m_telemetryPeer.clear(); // 清空对端地址

        if (m_commandPending) { // 有待确认命令时取消
            m_commandPending = false;
            m_pendingCommandSequence = 0;
            m_commandStatus = QStringLiteral("手机已连接，STM32 控制命令已取消");
            emit commandStateChanged(); // 通知命令状态变更
        }

        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort(); // 断开当前连接

        if (m_telemetryValid) { // 有有效遥测时标记为无效
            m_telemetryValid = false;
            emit telemetryChanged(); // 通知遥测状态变更
        }
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        qInfo() << "STM32 detection paused while phone device is active";
        return;
    }

    // 手机断开时恢复 STM32 检测
    setConnectionStatus(QStringLiteral("手机已断开，恢复 STM32 检测"));
    qInfo() << "Phone device released; resuming STM32 detection";
    QTimer::singleShot(0, this, &SensorBackend::attemptConnection); // 立即尝试连接
}

// 保存手机网页上报的温湿度并通知界面。
void SensorBackend::updateSensorData(double temperature, double humidity)
{
    m_temperature = temperature; // 更新温度
    m_humidity = humidity; // 更新湿度
    emit sensorChanged(); // 通知 QML 传感器数据已更新
}

// 根据活动 WLAN 的 IPv4 与掩码推导 AP 网关地址。
// 遍历 WiFi 网卡，用子网掩码计算第一个主机地址作为 AP 服务器地址。
QString SensorBackend::discoverApServerAddress() const
{
    QString fallback; // 兜底候选地址
    const auto interfaces = QNetworkInterface::allInterfaces(); // 获取所有网络接口
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags(); // 获取网卡状态标志
        if (!flags.testFlag(QNetworkInterface::IsUp) // 跳过未启用的网卡
            || !flags.testFlag(QNetworkInterface::IsRunning) // 跳过未运行的网卡
            || flags.testFlag(QNetworkInterface::IsLoopBack)) { // 跳过回环接口
            continue;
        }

        const QString name =
            (interface.name() + QLatin1Char(' ')
             + interface.humanReadableName()).toLower(); // 合并接口名
        const bool wifi = interface.type() == QNetworkInterface::Wifi // 判断是否为 WiFi 网卡
            || name.contains(QStringLiteral("wi-fi"))
            || name.contains(QStringLiteral("wlan"))
            || name.contains(QStringLiteral("无线"));
        if (!wifi) {
            continue; // 跳过非 WiFi 网卡
        }

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress localAddress = entry.ip(); // 获取本机 IP
            if (!usableIpv4(localAddress)) { // 跳过不可用的 IPv4
                continue;
            }

            const quint32 local = localAddress.toIPv4Address(); // 本机 IPv4 数值
            const quint32 mask = entry.netmask().toIPv4Address(); // 子网掩码数值
            if (mask == 0 || mask == 0xffffffffu) { // 无效掩码
                continue;
            }

            // AP 服务器通常占用 WLAN 子网的第一个主机地址。
            // 通过子网掩码计算，而非硬编码 192.168.x.x。
            const quint32 firstHost = (local & mask) + 1u; // 计算子网第一个主机地址
            if (firstHost != local) { // 排除本机自身
                const QString candidate =
                    QHostAddress(firstHost).toString(); // 候选 AP 地址
                if (localAddress.toString().startsWith(QStringLiteral("192.168.4."))) {
                    return candidate; // ESP8266 默认 AP 子网优先返回
                }
                if (fallback.isEmpty()) {
                    fallback = candidate; // 记录第一个兜底候选
                }
            }
        }
    }
    return fallback; // 返回兜底地址（可能为空）
}

// 根据当前目标配置尝试建立 STM32 TCP 连接。
// 自动模式下先发现 AP 网关地址，然后 connectToHost 并启动连接超时计时器。
void SensorBackend::attemptConnection()
{
    if (m_phoneDeviceActive) { // 手机活动时暂停检测
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) { // socket 非空闲状态
        return;
    }

    if (m_automaticTarget) { // 自动模式：发现 AP 网关地址
        const QString discovered = discoverApServerAddress(); // 查找 AP 服务器
        if (discovered.isEmpty()) { // 未发现可用 WLAN 热点
            m_targetHost.clear();
            setConnectionStatus(QStringLiteral("未发现可用 WLAN 热点，等待网络"));
            scheduleReconnect(); // 安排重连
            return;
        }
        m_targetHost = discovered; // 设置目标主机
        m_targetPort = kDefaultTelemetryPort; // 使用默认端口
    }

    if (m_targetHost.isEmpty()) { // 目标地址为空
        setConnectionStatus(QStringLiteral("STM32 服务器地址为空"));
        scheduleReconnect(); // 安排重连
        return;
    }

    m_buffer.clear(); // 清空接收缓冲区
    setConnectionStatus(QStringLiteral("正在连接 %1:%2")
                            .arg(m_targetHost)
                            .arg(m_targetPort));
    qInfo() << "Connecting to STM32 telemetry server"
            << m_targetHost << m_targetPort;
    m_socket->connectToHost(m_targetHost, m_targetPort); // 发起 TCP 连接
    m_connectTimeout->start(kConnectTimeoutMs); // 启动连接超时计时器
}

// 处理 TCP 建连成功并启动遥测看门狗。
void SensorBackend::socketConnected()
{
    if (m_phoneDeviceActive) { // 手机活动时立即断开
        m_socket->abort();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }

    m_connectTimeout->stop(); // 停止连接超时计时器
    m_retryTimer->stop(); // 停止重连计时器
    m_telemetryPeer = m_socket->peerAddress().toString(); // 记录对端地址
    setConnectionStatus(QStringLiteral("已连接，等待遥测数据"));
    m_telemetryWatchdog->start(); // 启动遥测看门狗
    qInfo() << "Connected to STM32 telemetry server:"
            << m_telemetryPeer << m_socket->peerPort();
}

// 清理断开的连接、待确认命令和接收缓存。
void SensorBackend::socketDisconnected()
{
    m_connectTimeout->stop(); // 停止连接超时计时器
    m_telemetryWatchdog->stop(); // 停止遥测看门狗
    if (m_phoneDeviceActive) { // 手机活动时清理并返回
        m_buffer.clear();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }
    if (m_commandPending) { // 有待确认命令时取消
        m_commandPending = false;
        m_pendingCommandSequence = 0;
        m_commandStatus = QStringLiteral("STM32 连接断开，待确认命令已取消");
        emit commandStateChanged(); // 通知命令状态变更
    }
    if (!m_buffer.trimmed().isEmpty()) { // 处理缓冲区中剩余的数据
        processTelemetryFrame(m_buffer.trimmed());
    }
    m_buffer.clear(); // 清空缓冲区
    setConnectionStatus(QStringLiteral("连接已断开，正在重连"));
    scheduleReconnect(); // 安排重连
}

// 处理 socket 错误并安排自动重连。
void SensorBackend::socketError()
{
    m_connectTimeout->stop(); // 停止连接超时计时器
    if (m_phoneDeviceActive) { // 手机活动时断开连接
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();
        setConnectionStatus(QStringLiteral("手机已连接，STM32 检测已暂停"));
        return;
    }
    const QString error = m_socket->errorString(); // 获取错误信息
    setConnectionStatus(QStringLiteral("连接失败：%1").arg(error));
    qWarning() << "STM32 telemetry connection error:" << error;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort(); // 中止当前连接
    }
    scheduleReconnect(); // 安排重连
}

// 在未被手机占用时安排下一次重连。
void SensorBackend::scheduleReconnect()
{
    if (m_phoneDeviceActive)
        return; // 手机活动时不重连
    if (!m_retryTimer->isActive()) {
        m_retryTimer->start(kRetryIntervalMs); // 启动重连计时器
    }
}

// 更新连接状态并仅在发生变化时通知 QML。
void SensorBackend::setConnectionStatus(const QString &status)
{
    if (m_telemetryStatus == status) {
        return; // 状态未变化，无需通知
    }
    m_telemetryStatus = status; // 更新状态
    emit telemetryConnectionChanged(); // 通知连接状态变更
}

// 立即中断当前连接并重新连接 STM32。
void SensorBackend::reconnectTelemetry()
{
    m_retryTimer->stop(); // 停止重连计时器
    m_connectTimeout->stop(); // 停止连接超时计时器
    m_socket->abort(); // 中止当前连接
    QTimer::singleShot(0, this, &SensorBackend::attemptConnection); // 立即尝试连接
}

// 切换到指定 IPv4 和端口的手动连接目标。
void SensorBackend::connectToTelemetry(const QString &host, int port)
{
    const QHostAddress address(host.trimmed()); // 解析地址
    if (address.isNull() || port < 1 || port > 65535) { // 地址或端口无效
        setConnectionStatus(QStringLiteral("手动服务器地址或端口无效"));
        return;
    }

    m_automaticTarget = false; // 切换到手动模式
    m_targetHost = address.toString(); // 设置目标地址
    m_targetPort = static_cast<quint16>(port); // 设置目标端口
    reconnectTelemetry(); // 重新连接
}

// 恢复根据 WLAN 子网自动推导 STM32 网关。
void SensorBackend::useAutomaticTelemetryTarget()
{
    m_automaticTarget = true; // 切换到自动模式
    reconnectTelemetry(); // 重新连接
}

// 校验参数并向 STM32 下发一条带序号的飞控命令。
// 支持 takeoff/hover/loiter/set_heading/rth/land/free_flight 等命令。
// 3 秒内未收到 ACK 则自动取消命令等待状态。
bool SensorBackend::sendFlightCommand(const QString &command,
                                      double headingValue,
                                      double targetAltitudeValue,
                                      double speedValue)
{
    static const QSet<QString> allowedCommands = { // 允许的命令白名单
        QStringLiteral("takeoff"),
        QStringLiteral("hover"),
        QStringLiteral("loiter"),
        QStringLiteral("set_heading"),
        QStringLiteral("rth"),
        QStringLiteral("land"),
        QStringLiteral("free_flight")
    };

    const QString normalizedCommand = command.trimmed().toLower(); // 规范化命令名
    if (!allowedCommands.contains(normalizedCommand)) { // 未知命令
        m_commandStatus = QStringLiteral("拒绝未知命令：%1").arg(command);
        emit commandStateChanged();
        return false;
    }
    if (m_socket->state() != QAbstractSocket::ConnectedState) { // TCP 未连接
        m_commandStatus = QStringLiteral("命令未发送：STM32 TCP 未连接");
        emit commandStateChanged();
        return false;
    }
    if (m_commandPending) { // 上一条命令仍在等待确认
        m_commandStatus = QStringLiteral("上一条命令仍在等待 STM32 确认");
        emit commandStateChanged();
        return false;
    }
    if (!qIsFinite(headingValue) || !qIsFinite(targetAltitudeValue) // 参数越界检查
        || !qIsFinite(speedValue)
        || headingValue < 0.0 || headingValue >= 360.0 // 航向 0~359°
        || targetAltitudeValue < 0.0 || targetAltitudeValue > 500.0 // 高度 0~500m
        || speedValue < 0.0 || speedValue > 50.0) { // 速度 0~50m/s
        m_commandStatus = QStringLiteral("命令参数越界：航向 0~359°、高度 0~500 m、速度 0~50 m/s");
        emit commandStateChanged();
        return false;
    }

    const bool autonomous = normalizedCommand == QStringLiteral("free_flight"); // 自由飞行=自主模式
    ++m_commandSequence; // 递增命令序号
    m_commandPending = true; // 标记命令等待确认
    m_pendingManualControl = !autonomous; // 非自主飞行=人工接管
    m_pendingCommandSequence = m_commandSequence; // 记录待确认序号

    // 构造飞控命令 JSON
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("flight_command"));
    object.insert(QStringLiteral("proto"), 1); // 协议版本
    object.insert(QStringLiteral("id"),
                  m_deviceId.isEmpty() ? QStringLiteral("UAV_01") : m_deviceId); // 设备 ID
    object.insert(QStringLiteral("command_seq"),
                  static_cast<double>(m_commandSequence)); // 命令序号
    object.insert(QStringLiteral("cmd"), normalizedCommand); // 命令类型
    object.insert(QStringLiteral("control"),
                  autonomous ? QStringLiteral("autonomous")
                             : QStringLiteral("manual")); // 控制模式
    object.insert(QStringLiteral("use_preset_route"), autonomous); // 是否使用预设航线
    object.insert(QStringLiteral("heading"), headingValue); // 航向角
    object.insert(QStringLiteral("target_alt"), targetAltitudeValue); // 目标高度
    object.insert(QStringLiteral("speed"), speedValue); // 飞行速度
    object.insert(QStringLiteral("ts"),
                  static_cast<double>(QDateTime::currentMSecsSinceEpoch())); // 时间戳

    QByteArray frame = QJsonDocument(object).toJson(QJsonDocument::Compact);
    frame.append("\r\n"); // 追加换行分隔符
    const qint64 written = m_socket->write(frame); // 通过 TCP 发送命令
    if (written != frame.size()) { // 写入失败
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
    emit commandStateChanged(); // 通知命令状态变更

    const qulonglong sentSequence = m_commandSequence; // 保存发送的序号
    // 三秒内没有匹配 ACK 时结束命令等待状态。
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

// 读取 TCP 缓冲区的遥测数据，按 \n 分隔为完整帧提交处理。
// 每次读取后重置看门狗，剩余数据留在缓冲区等待下次拼接。
void SensorBackend::readTelemetryData()
{
    if (m_phoneDeviceActive) { // 手机活动时忽略数据
        m_socket->readAll();
        m_buffer.clear(); // 清空缓冲区
        return;
    }

    m_buffer += m_socket->readAll(); // 追加到缓冲区
    if (m_buffer.size() > kMaximumBufferedBytes) { // 缓冲区溢出保护
        qWarning() << "STM32 telemetry buffer exceeded limit; clearing";
        m_buffer.clear(); // 清空缓冲区
        setConnectionStatus(QStringLiteral("接收缓存溢出，已清空"));
        return;
    }

    int newline = -1; // 换行符位置
    while ((newline = m_buffer.indexOf('\n')) >= 0) { // 逐个处理完整的行
        const QByteArray frame = m_buffer.left(newline).trimmed(); // 提取一行
        m_buffer.remove(0, newline + 1); // 从缓冲区移除已处理的行
        if (!frame.isEmpty()) {
            processTelemetryFrame(frame); // 处理遥测行
        }
    }

    if (!m_buffer.isEmpty() && m_buffer.trimmed().endsWith('}')) { // 缓冲区剩余完整 JSON 对象
        const QByteArray candidate = m_buffer.trimmed();
        if (processTelemetryFrame(candidate)) { // 处理成功则清空
            m_buffer.clear();
        }
    }
}

// 从 JSON 对象中安全读取一个 double 值，校验是否为有限值。
bool SensorBackend::finiteNumber(const QJsonObject &object,
                                 const char *key,
                                 double *value)
{
    const QJsonValue jsonValue = object.value(QLatin1String(key)); // 获取 JSON 值
    if (!jsonValue.isDouble()) { // 非数字类型则失败
        return false;
    }
    const double number = jsonValue.toDouble(qQNaN()); // 转为 double
    if (!qIsFinite(number)) { // 非有限值则失败
        return false;
    }
    *value = number; // 写入输出参数
    return true;
}

// 校验 STM32 遥测 JSON 帧，解析所有字段并更新内部状态。
// 处理命令 ACK 确认和遥测数据两种帧类型，更新传感器值、飞行状态并发出信号。
bool SensorBackend::processTelemetryFrame(const QByteArray &frame)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(frame, &error); // 解析 JSON
    if (error.error != QJsonParseError::NoError || !document.isObject()) { // JSON 无效
        setConnectionStatus(QStringLiteral("收到数据但 JSON 无效：%1")
                                .arg(error.errorString()));
        qWarning() << "Invalid STM32 telemetry JSON:"
                   << error.errorString() << frame.left(160);
        return false;
    }

    const QJsonObject object = document.object(); // 获取 JSON 对象
    const QString frameType = object.value(QStringLiteral("type")).toString(); // 帧类型字段
    if (frameType == QStringLiteral("command_ack") // 命令确认帧
        || frameType == QStringLiteral("ack")) {
        const QJsonValue sequenceValue = object.value(QStringLiteral("command_seq")); // 获取命令序号
        const qulonglong ackSequence = sequenceValue.isDouble()
            ? static_cast<qulonglong>(sequenceValue.toDouble()) : 0;
        const bool ok = object.value(QStringLiteral("ok")).toBool(false); // 是否成功
        const QString message = object.value(QStringLiteral("message")).toString(); // 附加消息

        if (ackSequence == 0) { // 无效序号
            // 缺陷/异常的 ACK 绝不能取消当前正在等待确认的有效命令。
            // 某些 ESP8266 固件可能交付来自先前 TCP 连接的缓冲响应。
            if (!m_commandPending) {
                m_commandStatus = QStringLiteral("已忽略无序命令确认（缺少 command_seq）");
                emit commandStateChanged(); // 通知命令状态变更
            }
            qWarning() << "STM32 command ACK without a valid sequence:" << frame;
            return true;
        }
        if (m_commandPending && ackSequence != m_pendingCommandSequence) { // 序号不匹配
            qWarning() << "Ignoring stale STM32 command ACK" << ackSequence
                       << "while waiting for" << m_pendingCommandSequence;
            return true;
        }

        if (ok) // 命令成功时应用人工控制模式
            m_manualControl = m_pendingManualControl;
        m_commandPending = false; // 清除命令等待状态
        m_pendingCommandSequence = 0;
        m_commandStatus = ok // 更新命令状态描述
            ? QStringLiteral("STM32 已确认命令 #%1%2")
                  .arg(ackSequence)
                  .arg(message.isEmpty() ? QString() : QStringLiteral("：") + message)
            : QStringLiteral("STM32 拒绝命令 #%1%2")
                  .arg(ackSequence)
                  .arg(message.isEmpty() ? QString() : QStringLiteral("：") + message);
        emit commandStateChanged(); // 通知命令状态变更
        return true;
    }
    const QString id = object.value(QStringLiteral("id")).toString(); // 设备 ID
    double lat = 0, lng = 0, headingValue = 0, targetAlt = 0; // 位置和姿态
    double speedValue = 0, rthLat = 0, rthLng = 0, rthDistanceValue = 0; // 速度和返航
    double dhtTemp = 0, dhtHum = 0, shtTemp = 0, shtHum = 0, mcuTemp = 0; // 传感器
    double proto = 0, seq = 0, mode = 0, alarm = 0, battery = 0, timestamp = 0; // 协议和状态

    const bool fieldsValid = !id.isEmpty() // 设备 ID 不能为空
        && finiteNumber(object, "proto", &proto) // 协议版本
        && finiteNumber(object, "seq", &seq) // 帧序号
        && finiteNumber(object, "lat", &lat) // 纬度
        && finiteNumber(object, "lng", &lng) // 经度
        && finiteNumber(object, "heading", &headingValue) // 航向角
        && finiteNumber(object, "target_alt", &targetAlt) // 目标高度
        && finiteNumber(object, "speed", &speedValue) // 速度
        && finiteNumber(object, "rth_lat", &rthLat) // 返航点纬度
        && finiteNumber(object, "rth_lng", &rthLng) // 返航点经度
        && finiteNumber(object, "rth_distance", &rthDistanceValue) // 返航点距离
        && finiteNumber(object, "dht_temp", &dhtTemp) // DHT 温度
        && finiteNumber(object, "dht_hum", &dhtHum) // DHT 湿度
        && finiteNumber(object, "sht_temp", &shtTemp) // SHT 温度
        && finiteNumber(object, "sht_hum", &shtHum) // SHT 湿度
        && finiteNumber(object, "mcu_temp", &mcuTemp) // MCU 温度
        && finiteNumber(object, "mode", &mode) // 飞行模式
        && finiteNumber(object, "alarm", &alarm) // 告警代码
        && finiteNumber(object, "battery", &battery) // 电量
        && finiteNumber(object, "ts", &timestamp); // 设备时间戳

    const bool valuesValid = fieldsValid // 字段解析成功
        && lat >= -90.0 && lat <= 90.0 // 纬度范围检查
        && lng >= -180.0 && lng <= 180.0 // 经度范围检查
        && rthLat >= -90.0 && rthLat <= 90.0 // 返航点纬度范围
        && rthLng >= -180.0 && rthLng <= 180.0 // 返航点经度范围
        && speedValue >= 0.0 && rthDistanceValue >= 0.0 // 非负值检查
        && dhtTemp >= -50.0 && dhtTemp <= 100.0 // DHT 温度范围
        && dhtHum >= 0.0 && dhtHum <= 100.0 // DHT 湿度范围
        && shtTemp >= -50.0 && shtTemp <= 100.0 // SHT 温度范围
        && shtHum >= 0.0 && shtHum <= 100.0 // SHT 湿度范围
        && battery >= 0.0 && battery <= 100.0 // 电量范围
        && seq >= 0.0; // 序号非负

    if (!valuesValid) { // 字段缺失或越界
        setConnectionStatus(QStringLiteral("JSON 字段缺失或数值越界"));
        qWarning() << "STM32 telemetry frame has missing or invalid fields";
        return false;
    }

    m_deviceId = id; // 更新设备 ID
    m_protocolVersion = qRound(proto); // 更新协议版本
    m_sequence = static_cast<qulonglong>(seq); // 更新帧序号
    m_stm32Latitude = lat; // 更新 STM32 纬度
    m_stm32Longitude = lng; // 更新 STM32 经度
    m_heading = std::fmod(std::fmod(headingValue, 360.0) + 360.0, 360.0); // 归一化航向 [0, 360)
    m_targetAltitude = targetAlt; // 更新目标高度
    m_stm32Speed = speedValue; // 更新飞行速度
    m_rthLatitude = rthLat; // 更新返航点纬度
    m_rthLongitude = rthLng; // 更新返航点经度
    m_rthDistance = rthDistanceValue; // 更新返航点距离
    m_dhtTemperature = dhtTemp; // 更新 DHT 温度
    m_dhtHumidity = dhtHum; // 更新 DHT 湿度
    m_shtTemperature = shtTemp; // 更新 SHT 温度
    m_shtHumidity = shtHum; // 更新 SHT 湿度
    m_mcuTemperature = mcuTemp; // 更新 MCU 温度
    m_flightMode = qRound(mode); // 更新飞行模式
    m_alarmCode = qRound(alarm); // 更新告警代码
    m_stm32Battery = qRound(battery); // 更新电量
    m_telemetryTimestamp = static_cast<qulonglong>(timestamp); // 更新设备时间戳
    m_telemetryUpdateTime =
        QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")); // 刷新本机更新时间
    m_telemetryValid = true; // 标记遥测有效
    m_telemetryWatchdog->start(); // 重启看门狗计时
    m_telemetryPeer = m_socket->peerAddress().toString(); // 更新对端地址
    setConnectionStatus(QStringLiteral("遥测接收正常")); // 更新连接状态

    if (m_sequence % 10u == 0u) { // 每 10 帧记录一次日志
        qInfo() << "STM32 telemetry received:"
                << m_deviceId << "seq" << m_sequence
                << "position" << m_stm32Latitude << m_stm32Longitude;
    }
    emit telemetryChanged(); // 通知 QML 遥测数据已更新
    return true;
}