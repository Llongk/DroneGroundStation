#include "cloudbackend.h"

#include "backend.h"
#include "historydatabase.h"
#include "sensorbackend.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QDebug>
#include <QStringList>

namespace {
constexpr int kReconnectDelayMs = 5000;
constexpr int kHttpRetryDelayMs = 5000;
constexpr int kKeepAliveSeconds = 30;
constexpr int kMaximumPacketSize = 1024 * 1024;

// 生成可用于 MQTT Client ID 的安全字符文本（仅允许字母数字连字符和下划线）。
QString normalizedClientId(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), // 替换非法字符
                  QStringLiteral("-"));
    return value.left(64); // 截断到 64 字符
}
}

CloudBackend::CloudBackend(Backend *backend,
                           SensorBackend *sensorBackend,
                           HistoryDatabase *historyDatabase,
                           QObject *parent)
    : QObject(parent),
      m_backend(backend),
      m_sensorBackend(sensorBackend),
      m_historyDatabase(historyDatabase),
      m_socket(new QSslSocket(this)),
      m_http(new QNetworkAccessManager(this)),
      m_reconnectTimer(new QTimer(this)),
      m_keepAliveTimer(new QTimer(this)),
      m_httpRetryTimer(new QTimer(this)),
      m_connectionName(QStringLiteral("cloud_outbox_%1")
                           .arg(reinterpret_cast<quintptr>(this)))
{
    m_clientId = normalizedClientId(QStringLiteral("DGS-%1-%2")
                                        .arg(QSysInfo::machineHostName(),
                                             QString::number(QRandomGenerator::global()->generate(), 16)));
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kReconnectDelayMs);
    m_keepAliveTimer->setInterval((kKeepAliveSeconds * 1000) / 2);
    m_httpRetryTimer->setSingleShot(true);
    m_httpRetryTimer->setInterval(kHttpRetryDelayMs);

    connect(m_reconnectTimer, &QTimer::timeout, this, &CloudBackend::connectToCloud);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &CloudBackend::sendKeepAlive);
    connect(m_httpRetryTimer, &QTimer::timeout, this, &CloudBackend::pumpHttpOutbox);
    connect(m_http, &QNetworkAccessManager::finished, this, &CloudBackend::httpFinished);
    connect(m_socket, &QSslSocket::encrypted, this, &CloudBackend::tlsEncrypted);
    connect(m_socket, &QSslSocket::readyRead, this, &CloudBackend::socketReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, &CloudBackend::socketDisconnected);
    connect(m_socket, &QSslSocket::errorOccurred, this, &CloudBackend::socketError);
    connect(m_socket, &QSslSocket::sslErrors, this,
            [this](const QList<QSslError> &errors) {
        QStringList messages;
        for (const QSslError &error : errors)
            messages.append(error.errorString());
        setStatus(QStringLiteral("云端 TLS 校验失败：%1").arg(messages.join(QStringLiteral("；"))));
        m_socket->abort();
    });

    openOutbox();
    loadConfiguration();
    connect(m_backend, &Backend::gpsChanged, this, &CloudBackend::recordPhoneGps);
    connect(m_sensorBackend, &SensorBackend::sensorChanged,
            this, &CloudBackend::recordPhoneSensor);
    connect(m_sensorBackend, &SensorBackend::telemetryChanged,
            this, &CloudBackend::recordStm32Telemetry);

    if (m_enabled)
        QTimer::singleShot(0, this, &CloudBackend::connectToCloud);
    QTimer::singleShot(0, this, &CloudBackend::pumpHttpOutbox);
}

CloudBackend::~CloudBackend()
{
    m_enabled = false;
    m_socket->abort();
    if (m_database.isOpen())
        m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool CloudBackend::enabled() const { return m_enabled; } // 返回是否启用云端上传
bool CloudBackend::connected() const { return m_mqttConnected; } // 返回是否已 TLS 连接
QString CloudBackend::status() const { return m_status; } // 返回当前云端状态文本
QString CloudBackend::host() const { return m_host; } // 返回 EMQX 主机地址
int CloudBackend::port() const { return m_port; } // 返回 EMQX 端口
QString CloudBackend::username() const { return m_username; } // 返回 EMQX 用户名
QString CloudBackend::clientId() const { return m_clientId; } // 返回 MQTT Client ID
int CloudBackend::pendingCount() const { return m_pendingCount; } // 返回待上传消息数

// 打开或创建云端离线队列 SQLite 数据库，共享历史数据库路径。
// 创建 cloud_outbox 表并支持 HTTP 历史上传扩展字段。
bool CloudBackend::openOutbox()
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName); // 创建数据库连接
    m_database.setDatabaseName(m_historyDatabase->databasePath()); // 使用历史数据库路径
    if (!m_database.open()) { // 打开数据库失败
        setStatus(QStringLiteral("无法打开云端离线队列：%1")
                      .arg(m_database.lastError().text()));
        return false;
    }
    QSqlQuery query(m_database); // 创建查询对象
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL")); // 使用 WAL 模式提高并发
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL")); // 写入同步策略
    if (!query.exec(QStringLiteral(R"SQL(
        CREATE TABLE IF NOT EXISTS cloud_outbox (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id TEXT NOT NULL UNIQUE,
            topic TEXT NOT NULL,
            payload TEXT NOT NULL,
            created_ms INTEGER NOT NULL,
            retry_count INTEGER NOT NULL DEFAULT 0,
            uploaded INTEGER NOT NULL DEFAULT 0
        )
    )SQL"))) { // 创建离线队列表
        setStatus(QStringLiteral("无法创建云端离线队列：%1").arg(query.lastError().text()));
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cloud_pending ON cloud_outbox(uploaded, id)")); // 创建待上传索引
    bool hasHttpUploaded = false; // 是否已有 http_uploaded 列
    if (query.exec(QStringLiteral("PRAGMA table_info(cloud_outbox)"))) {
        while (query.next()) { // 遍历列信息
            if (query.value(1).toString() == QStringLiteral("http_uploaded")) {
                hasHttpUploaded = true;
                break;
            }
        }
    }
    if (!hasHttpUploaded
        && !query.exec(QStringLiteral(
            "ALTER TABLE cloud_outbox ADD COLUMN http_uploaded INTEGER NOT NULL DEFAULT 0"))) { // 添加 HTTP 上传标记列
        setStatus(QStringLiteral("无法升级云端历史队列：%1").arg(query.lastError().text()));
        return false;
    }
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_cloud_http_pending ON cloud_outbox(http_uploaded, id)")); // 创建 HTTP 待上传索引

    // 重启后立即暴露最新持久化遥测，使云端仪表盘在旧积压排空期间仍有可用数据。
    if (query.exec(QStringLiteral(
            "SELECT id FROM cloud_outbox "
            "WHERE http_uploaded=0 ORDER BY created_ms DESC, id DESC LIMIT 1"))
        && query.next()) {
        m_httpPriorityRowId = query.value(0).toLongLong(); // 记录最新待上传行 ID
    }
    updatePendingCount(); // 更新待处理计数
    return true;
}

// 返回配置目录路径（数据库目录上级的 config 子目录）。
QString CloudBackend::configurationDirectory() const
{
    QDir databaseDirectory(QFileInfo(m_historyDatabase->databasePath()).absolutePath()); // 数据库所在目录
    databaseDirectory.cdUp(); // 上一级目录
    return databaseDirectory.filePath(QStringLiteral("config")); // 返回 config 子目录
}

// 从 config/cloud.json 加载云端配置，填充到成员变量。
void CloudBackend::loadConfiguration()
{
    QFile file(QDir(configurationDirectory()).filePath(QStringLiteral("cloud.json"))); // 打开配置文件
    if (!file.open(QIODevice::ReadOnly))
        return; // 文件不存在则跳过
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object(); // 读取 JSON
    m_host = object.value(QStringLiteral("host")).toString(m_host).trimmed(); // 主机地址
    m_port = static_cast<quint16>(object.value(QStringLiteral("port")).toInt(m_port)); // 端口
    m_username = object.value(QStringLiteral("username")).toString(m_username); // 用户名
    m_password = object.value(QStringLiteral("password")).toString(); // 密码
    m_clientId = normalizedClientId(object.value(QStringLiteral("clientId")).toString(m_clientId)); // 客户端 ID
    m_historyIngestUrl = object.value(QStringLiteral("historyIngestUrl"))
        .toString(m_historyIngestUrl).trimmed(); // 历史上传 URL
    m_historyIngestKey = object.value(QStringLiteral("historyIngestKey")).toString().trimmed(); // 历史上传密钥
    m_enabled = object.value(QStringLiteral("enabled")).toBool(false) && !m_password.isEmpty(); // 密码非空才启用
    if (m_enabled)
        m_status = QStringLiteral("等待连接 EMQX Cloud"); // 初始状态
}

// 保存当前云端配置到 config/cloud.json。
bool CloudBackend::saveConfiguration() const
{
    QDir directory(configurationDirectory()); // 获取配置目录
    if (!directory.mkpath(QStringLiteral("."))) // 确保目录存在
        return false;
    QJsonObject object; // 构造配置 JSON
    object.insert(QStringLiteral("host"), m_host);
    object.insert(QStringLiteral("port"), m_port);
    object.insert(QStringLiteral("username"), m_username);
    object.insert(QStringLiteral("password"), m_password);
    object.insert(QStringLiteral("clientId"), m_clientId);
    object.insert(QStringLiteral("historyIngestUrl"), m_historyIngestUrl);
    object.insert(QStringLiteral("historyIngestKey"), m_historyIngestKey);
    object.insert(QStringLiteral("enabled"), m_enabled);
    QFile file(directory.filePath(QStringLiteral("cloud.json"))); // 打开文件
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) // 截断写入
        && file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}

// 由 QML 界面调用，校验并保存 EMQX 云端配置。
bool CloudBackend::configure(const QString &host,
                             int port,
                             const QString &username,
                             const QString &password,
                             const QString &clientId,
                             bool enabled)
{
    if (host.trimmed().isEmpty() || port < 1 || port > 65535 // 校验主机和端口
        || username.trimmed().isEmpty() || clientId.trimmed().isEmpty()) { // 校验用户名和客户端 ID
        setStatus(QStringLiteral("云端配置不完整"));
        return false;
    }
    if (enabled && password.isEmpty() && m_password.isEmpty()) { // 需要密码但未提供
        setStatus(QStringLiteral("请输入 EMQX 认证密码"));
        return false;
    }
    m_host = host.trimmed(); // 保存主机
    m_port = static_cast<quint16>(port); // 保存端口
    m_username = username.trimmed(); // 保存用户名
    if (!password.isEmpty())
        m_password = password; // 仅当提供密码时更新
    m_clientId = normalizedClientId(clientId.trimmed()); // 规范化并保存客户端 ID
    m_enabled = enabled; // 保存启用状态
    emit configurationChanged(); // 通知配置变更
    emit stateChanged(); // 通知状态变更
    if (!saveConfiguration()) { // 持久化配置失败
        setStatus(QStringLiteral("无法保存 config/cloud.json"));
        return false;
    }
    reconnect(); // 按新配置重连
    return true;
}

// 断开现有连接并重新连接云端。
void CloudBackend::reconnect()
{
    m_reconnectTimer->stop(); // 停止重连计时器
    m_keepAliveTimer->stop(); // 停止心跳计时器
    m_mqttConnected = false; // 标记已断开
    m_inFlight.clear(); // 清空已发送未确认的消息
    m_socket->abort(); // 中止 socket 连接
    emit stateChanged(); // 通知状态变更
    if (m_enabled)
        QTimer::singleShot(0, this, &CloudBackend::connectToCloud); // 立即连接
    else
        setStatus(QStringLiteral("云端上传已关闭"));
}

// 通过 TLS 加密连接到 EMQX Cloud 主机。
void CloudBackend::connectToCloud()
{
    if (!m_enabled || m_host.isEmpty() || m_password.isEmpty()) // 未启用或配置不完整
        return;
    setStatus(QStringLiteral("正在通过 TLS 连接 %1:%2").arg(m_host).arg(m_port));
    const QString caPath = QDir(configurationDirectory()).filePath(QStringLiteral("emqxsl-ca.crt")); // CA 证书路径
    const QList<QSslCertificate> certificates = QSslCertificate::fromPath(caPath); // 加载 CA 证书
    if (certificates.isEmpty()) { // CA 证书无效
        setStatus(QStringLiteral("未找到有效 CA：%1").arg(caPath));
        scheduleReconnect(); // 安排重连
        return;
    }
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration(); // 默认 SSL 配置
    QList<QSslCertificate> authorities = ssl.caCertificates(); // 系统 CA 证书
    authorities.append(certificates); // 追加项目 CA 证书
    ssl.setCaCertificates(authorities); // 设置受信任 CA
    m_socket->setSslConfiguration(ssl); // 应用 SSL 配置
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer); // 验证对端证书
    m_socket->setPeerVerifyName(m_host); // 验证主机名
    m_socket->connectToHostEncrypted(m_host, m_port); // 发起 TLS 加密连接
}

// TLS 握手完成后发送 MQTT CONNECT 报文。
void CloudBackend::tlsEncrypted()
{
    setStatus(QStringLiteral("TLS 已建立，正在进行 MQTT 认证"));
    sendConnectPacket(); // 发送 MQTT 连接请求
}

// 构造并发送 MQTT CONNECT 报文（含用户名/密码认证）。
void CloudBackend::sendConnectPacket()
{
    QByteArray body;
    appendMqttString(body, QByteArrayLiteral("MQTT")); // 协议名
    body.append(char(4)); // 协议版本 3.1.1
    body.append(char(0xC2)); // 标志位：用户名 + 密码 + 清理会话
    body.append(char((kKeepAliveSeconds >> 8) & 0xff)); // 心跳间隔高字节
    body.append(char(kKeepAliveSeconds & 0xff)); // 心跳间隔低字节
    appendMqttString(body, m_clientId.toUtf8()); // 客户端 ID
    appendMqttString(body, m_username.toUtf8()); // 用户名
    appendMqttString(body, m_password.toUtf8()); // 密码
    m_socket->write(mqttPacket(0x10, body)); // 通过 TLS 发送 CONNECT 报文
}

// 接收 TLS 数据并调用 MQTT 报文解析循环。
void CloudBackend::socketReadyRead()
{
    m_buffer.append(m_socket->readAll()); // 追加到缓冲区
    if (m_buffer.size() > kMaximumPacketSize) { // 缓冲区溢出保护
        setStatus(QStringLiteral("云端 MQTT 数据包超过安全上限"));
        m_socket->abort(); // 中止连接
        return;
    }
    processPackets(); // 解析 MQTT 报文
}

// 循环解析 MQTT 报文（固定头 + 变长长度 + 负载）。
void CloudBackend::processPackets()
{
    while (m_buffer.size() >= 2) { // 至少需要固定头和长度字节
        const quint8 header = static_cast<quint8>(m_buffer.at(0)); // 固定头
        int multiplier = 1; // 变长编码乘数
        int remaining = 0; // 剩余长度
        int index = 1; // 当前读取位置
        quint8 encoded = 0; // 编码字节
        do {
            if (index >= m_buffer.size()) // 数据不完整
                return;
            encoded = static_cast<quint8>(m_buffer.at(index++)); // 读取变长编码字节
            remaining += (encoded & 127) * multiplier; // 累加剩余长度
            multiplier *= 128; // 下一字节乘数×128
            if (multiplier > 128 * 128 * 128 * 128) { // 防止变长解码溢出
                m_socket->abort();
                return;
            }
        } while (encoded & 128); // 最高位为 1 表示继续
        if (m_buffer.size() < index + remaining) // 负载不完整
            return;
        const QByteArray body = m_buffer.mid(index, remaining); // 提取负载
        m_buffer.remove(0, index + remaining); // 从缓冲区移除已处理报文
        processPacket(header, body); // 处理单个报文
    }
}

// 处理单个 MQTT 报文，支持 CONNACK、PUBACK、PINGRESP。
void CloudBackend::processPacket(quint8 header, const QByteArray &body)
{
    const int type = header >> 4; // 报文类型（高 4 位）
    if (type == 2) { // CONNACK
        if (body.size() < 2 || static_cast<quint8>(body.at(1)) != 0) { // 连接失败
            const int code = body.size() >= 2 ? static_cast<quint8>(body.at(1)) : -1; // 获取返回码
            setStatus(QStringLiteral("EMQX MQTT 认证失败，返回码 %1").arg(code));
            m_socket->disconnectFromHost(); // 断开连接
            return;
        }
        m_mqttConnected = true; // 标记已连接
        m_keepAliveTimer->start(); // 启动心跳
        setStatus(QStringLiteral("EMQX 云端已连接"));
        emit stateChanged(); // 通知状态变更
        pumpOutbox(); // 推送离线队列
        return;
    }
    if (type == 4 && body.size() >= 2) { // PUBACK
        const quint16 id = (static_cast<quint8>(body.at(0)) << 8) // 获取包 ID 高字节
            | static_cast<quint8>(body.at(1)); // 获取包 ID 低字节
        if (m_inFlight.contains(id)) // 匹配已发送消息
            markUploaded(m_inFlight.take(id)); // 标记已上传
        pumpOutbox(); // 继续推送队列
        return;
    }
    if (type == 13) // PINGRESP
        return;
}

// 处理 TLS 连接断开，重置 MQTT 状态并安排重连。
void CloudBackend::socketDisconnected()
{
    const bool wasConnected = m_mqttConnected; // 记录之前是否已连接
    m_mqttConnected = false; // 标记已断开
    m_keepAliveTimer->stop(); // 停止心跳
    m_inFlight.clear(); // 清空已发送未确认的消息
    if (wasConnected)
        setStatus(QStringLiteral("云端连接已断开，数据将在本地排队"));
    emit stateChanged(); // 通知状态变更
    scheduleReconnect(); // 安排重连
}

// 处理 socket 错误并安排重连。
void CloudBackend::socketError()
{
    if (!m_enabled) // 未启用时忽略
        return;
    setStatus(QStringLiteral("云端连接错误：%1").arg(m_socket->errorString()));
    scheduleReconnect(); // 安排重连
}

// 在启用状态下启动重连计时器。
void CloudBackend::scheduleReconnect()
{
    if (m_enabled && !m_reconnectTimer->isActive()) // 启用且计时器未启动
        m_reconnectTimer->start();
}

// 发送 MQTT PINGREQ 心跳保持连接。
void CloudBackend::sendKeepAlive()
{
    if (m_mqttConnected) // 仅连接时发送
        m_socket->write(QByteArray::fromHex("c000")); // PINGREQ 报文
}

// 将手机 GPS 数据写入云端离线队列。
void CloudBackend::recordPhoneGps() { enqueue(QStringLiteral("dgs/UAV_01/phone/telemetry"), phonePayload(QStringLiteral("gps"))); }
// 将手机温湿度数据写入云端离线队列。
void CloudBackend::recordPhoneSensor() { enqueue(QStringLiteral("dgs/UAV_01/phone/telemetry"), phonePayload(QStringLiteral("sensor"))); }
// 将 STM32 遥测数据写入云端离线队列。
void CloudBackend::recordStm32Telemetry() { enqueue(QStringLiteral("dgs/UAV_01/stm32/telemetry"), stm32Payload()); }

// 构造手机遥测 JSON 负载（GPS 或传感器事件）。
QByteArray CloudBackend::phonePayload(const QString &eventType) const
{
    QJsonObject object;
    object.insert(QStringLiteral("message_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)); // 唯一消息 ID
    object.insert(QStringLiteral("source"), QStringLiteral("phone")); // 数据来源
    object.insert(QStringLiteral("event_type"), eventType); // 事件类型
    object.insert(QStringLiteral("session_id"), m_historyDatabase->currentSessionId()); // 飞行会话 ID
    object.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()); // 时间戳
    object.insert(QStringLiteral("latitude"), m_backend->latitude()); // 纬度
    object.insert(QStringLiteral("longitude"), m_backend->longitude()); // 经度
    object.insert(QStringLiteral("altitude"), m_backend->height()); // 高度
    object.insert(QStringLiteral("speed"), m_backend->speed()); // 速度
    object.insert(QStringLiteral("battery"), m_backend->battery()); // 电量
    object.insert(QStringLiteral("pitch"), m_backend->pitch()); // 俯仰角
    object.insert(QStringLiteral("roll"), m_backend->roll()); // 横滚角
    object.insert(QStringLiteral("yaw"), m_backend->yaw()); // 偏航角
    object.insert(QStringLiteral("accuracy"), m_backend->accuracy()); // 定位精度
    object.insert(QStringLiteral("temperature"), m_sensorBackend->temperature()); // 温度
    object.insert(QStringLiteral("humidity"), m_sensorBackend->humidity()); // 湿度
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// 构造 STM32 遥测 JSON 负载。
QByteArray CloudBackend::stm32Payload() const
{
    QJsonObject object;
    object.insert(QStringLiteral("message_id"), QStringLiteral("%1-%2-%3")
                      .arg(m_historyDatabase->currentSessionId(), // 会话 ID
                           m_sensorBackend->deviceId()) // 设备 ID
                      .arg(m_sensorBackend->sequence())); // 帧序号
    object.insert(QStringLiteral("source"), QStringLiteral("stm32")); // 数据来源
    object.insert(QStringLiteral("session_id"), m_historyDatabase->currentSessionId()); // 飞行会话 ID
    object.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()); // 时间戳
    object.insert(QStringLiteral("device_id"), m_sensorBackend->deviceId()); // 设备 ID
    object.insert(QStringLiteral("sequence"), static_cast<qint64>(m_sensorBackend->sequence())); // 帧序号
    object.insert(QStringLiteral("latitude"), m_sensorBackend->stm32Latitude()); // 纬度
    object.insert(QStringLiteral("longitude"), m_sensorBackend->stm32Longitude()); // 经度
    object.insert(QStringLiteral("heading"), m_sensorBackend->heading()); // 航向角
    object.insert(QStringLiteral("target_altitude"), m_sensorBackend->targetAltitude()); // 目标高度
    object.insert(QStringLiteral("speed"), m_sensorBackend->stm32Speed()); // 速度
    object.insert(QStringLiteral("rth_latitude"), m_sensorBackend->rthLatitude()); // 返航点纬度
    object.insert(QStringLiteral("rth_longitude"), m_sensorBackend->rthLongitude()); // 返航点经度
    object.insert(QStringLiteral("rth_distance"), m_sensorBackend->rthDistance()); // 返航点距离
    object.insert(QStringLiteral("dht_temperature"), m_sensorBackend->dhtTemperature()); // DHT 温度
    object.insert(QStringLiteral("dht_humidity"), m_sensorBackend->dhtHumidity()); // DHT 湿度
    object.insert(QStringLiteral("sht_temperature"), m_sensorBackend->shtTemperature()); // SHT 温度
    object.insert(QStringLiteral("sht_humidity"), m_sensorBackend->shtHumidity()); // SHT 湿度
    object.insert(QStringLiteral("mcu_temperature"), m_sensorBackend->mcuTemperature()); // MCU 温度
    object.insert(QStringLiteral("flight_mode"), m_sensorBackend->flightMode()); // 飞行模式
    object.insert(QStringLiteral("alarm_code"), m_sensorBackend->alarmCode()); // 告警代码
    object.insert(QStringLiteral("battery"), m_sensorBackend->stm32Battery()); // 电量
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// 将遥测数据写入云端离线队列数据库，优先通过 MQTT 推送，其次通过 HTTP 历史上传。
void CloudBackend::enqueue(const QString &topic, const QByteArray &payload)
{
    if (!m_database.isOpen() // 数据库未打开
        || m_historyDatabase->currentSessionId().isEmpty()) // 无当前会话
        return;
    const QJsonObject object = QJsonDocument::fromJson(payload).object(); // 解析负载
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO cloud_outbox(message_id,topic,payload,created_ms) VALUES(?,?,?,?)"));
    query.addBindValue(object.value(QStringLiteral("message_id")).toString()); // 消息 ID
    query.addBindValue(topic); // MQTT 主题
    query.addBindValue(QString::fromUtf8(payload)); // 负载 JSON
    query.addBindValue(QDateTime::currentMSecsSinceEpoch()); // 创建时间
    if (!query.exec()) {
        qWarning() << "Cannot enqueue cloud message:" << query.lastError().text();
    } else if (query.numRowsAffected() > 0) {
        // 不要让实时手机/STM32 遥测等待大量旧积压。
        // 下一个 HTTP 槽先发送最新行；之后按正常旧数据优先查询继续排空持久化记录。
        m_httpPriorityRowId = query.lastInsertId().toLongLong(); // 优先发送最新行
    }
    updatePendingCount(); // 更新待处理计数
    pumpOutbox(); // 尝试通过 MQTT 推送
    pumpHttpOutbox(); // 尝试通过 HTTP 上传
}

// 从离线队列取一条未上传记录，通过 HTTP POST 历史上传接口发送。
// 优先发送最新记录，再按旧数据顺序排空积压。
void CloudBackend::pumpHttpOutbox()
{
    if (m_httpInFlightRowId != 0 || !m_database.isOpen() // 有上传进行中或数据库未打开
        || m_historyIngestUrl.isEmpty() || m_historyIngestKey.isEmpty()) { // URL 或密钥缺失
        return;
    }

    QSqlQuery query(m_database);
    bool rowAvailable = false;
    if (m_httpPriorityRowId > 0) { // 优先发送最新记录
        query.prepare(QStringLiteral(
            "SELECT id,topic,payload FROM cloud_outbox "
            "WHERE id=? AND http_uploaded=0"));
        query.addBindValue(m_httpPriorityRowId);
        rowAvailable = query.exec() && query.next();
        if (!rowAvailable)
            m_httpPriorityRowId = 0; // 记录已上传或不存在
    }

    if (!rowAvailable) { // 按旧数据顺序取最早未上传记录
        rowAvailable = query.exec(QStringLiteral(
            "SELECT id,topic,payload FROM cloud_outbox "
            "WHERE http_uploaded=0 ORDER BY id LIMIT 1"))
            && query.next();
    }
    if (!rowAvailable) // 无待上传记录
        return;

    const qint64 rowId = query.value(0).toLongLong(); // 行 ID
    const QString topic = query.value(1).toString(); // MQTT 主题
    const QJsonObject payload = QJsonDocument::fromJson(
        query.value(2).toString().toUtf8()).object(); // 负载 JSON
    if (payload.isEmpty()) { // 负载为空
        qWarning() << "Cannot upload empty cloud history payload for row" << rowId;
        return;
    }

    QJsonObject envelope; // 历史上传信封
    envelope.insert(QStringLiteral("topic"), topic);
    envelope.insert(QStringLiteral("payload"), payload);
    envelope.insert(QStringLiteral("clientid"), m_clientId);
    envelope.insert(QStringLiteral("qos"), 1);
    envelope.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    QUrl url(m_historyIngestUrl); // 历史上传 URL
    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(QStringLiteral("key"), m_historyIngestKey); // 追加密钥查询参数
    url.setQuery(urlQuery);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")); // JSON 内容类型
    request.setTransferTimeout(15000); // 15 秒超时
    QNetworkReply *reply = m_http->post(
        request, QJsonDocument(envelope).toJson(QJsonDocument::Compact)); // 发送 POST 请求
    reply->setProperty("cloudHistoryRowId", rowId); // 记录行 ID 用于回调
    m_httpInFlightRowId = rowId; // 标记正在上传的行
}

// 处理 HTTP 历史上传响应，成功时标记已上传，失败时安排重试。
void CloudBackend::httpFinished(QNetworkReply *reply)
{
    const qint64 rowId = reply->property("cloudHistoryRowId").toLongLong(); // 获取行 ID
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt(); // HTTP 状态码
    const QByteArray responseBody = reply->readAll(); // 响应体
    const bool succeeded = reply->error() == QNetworkReply::NoError // 无网络错误
        && statusCode >= 200 && statusCode < 300; // 2xx 状态码

    if (succeeded && rowId > 0) { // 上传成功
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "UPDATE cloud_outbox SET http_uploaded=1 WHERE id=?")); // 标记已上传
        query.addBindValue(rowId);
        if (!query.exec())
            qWarning() << "Cannot mark D1 history row uploaded:" << query.lastError().text();
        if (rowId == m_httpPriorityRowId) // 清除优先行 ID
            m_httpPriorityRowId = 0;
    } else {
        qWarning() << "D1 history upload failed:"
                   << "HTTP" << statusCode
                   << reply->errorString()
                   << responseBody.left(256);
    }

    reply->deleteLater(); // 释放网络回复
    m_httpInFlightRowId = 0; // 清除上传中标记
    if (succeeded)
        QTimer::singleShot(0, this, &CloudBackend::pumpHttpOutbox); // 继续上传下一行
    else if (!m_httpRetryTimer->isActive()) // 失败时启动重试计时器
        m_httpRetryTimer->start();
}

// 从离线队列取一条记录通过 MQTT QoS 1 发布到 EMQX。
void CloudBackend::pumpOutbox()
{
    if (!m_mqttConnected || !m_inFlight.isEmpty() || !m_database.isOpen()) // 前置条件检查
        return;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id,topic,payload FROM cloud_outbox WHERE uploaded=0 ORDER BY id LIMIT 1")) // 取最早未上传记录
        || !query.next())
        return;
    const qint64 rowId = query.value(0).toLongLong(); // 行 ID
    const QByteArray topic = query.value(1).toString().toUtf8(); // 主题
    const QByteArray payload = query.value(2).toString().toUtf8(); // 负载
    const quint16 id = nextPacketId(); // 分配包 ID
    QByteArray body;
    appendMqttString(body, topic); // 主题
    body.append(char((id >> 8) & 0xff)); // 包 ID 高字节
    body.append(char(id & 0xff)); // 包 ID 低字节
    body.append(payload); // 负载
    m_inFlight.insert(id, rowId); // 记录已发送消息
    m_socket->write(mqttPacket(0x32, body)); // 发送 PUBLISH 报文 (QoS 1)
    QSqlQuery retry(m_database);
    retry.prepare(QStringLiteral("UPDATE cloud_outbox SET retry_count=retry_count+1 WHERE id=?")); // 递增重试计数
    retry.addBindValue(rowId);
    retry.exec();
}

// 标记离线队列记录为已上传。
void CloudBackend::markUploaded(qint64 rowId)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE cloud_outbox SET uploaded=1 WHERE id=?"));
    query.addBindValue(rowId);
    query.exec();
    updatePendingCount(); // 更新待处理计数
}

// 查询数据库更新待上传消息计数。
void CloudBackend::updatePendingCount()
{
    if (!m_database.isOpen())
        return;
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM cloud_outbox WHERE uploaded=0")) && query.next()) {
        const int count = query.value(0).toInt(); // 获取计数
        if (count != m_pendingCount) { // 计数变化时通知
            m_pendingCount = count;
            emit pendingCountChanged(); // 通知待处理计数变更
        }
    }
}

// 生成下一个 MQTT 包 ID（1~65535 循环）。
quint16 CloudBackend::nextPacketId()
{
    if (++m_packetId == 0) // 递增后为 0 时跳过
        ++m_packetId;
    return m_packetId;
}

// 构造 MQTT 报文（固定头 + 变长长度 + 负载）。
QByteArray CloudBackend::mqttPacket(quint8 header, const QByteArray &body) const
{
    QByteArray packet;
    packet.append(char(header)); // 固定头
    int length = body.size(); // 变长编码长度
    do {
        quint8 encoded = length % 128; // 低 7 位
        length /= 128;
        if (length > 0)
            encoded |= 128; // 设置延续位
        packet.append(char(encoded)); // 追加编码字节
    } while (length > 0);
    packet.append(body); // 追加负载
    return packet;
}

// 按 MQTT UTF-8 字符串格式追加到目标缓冲区（2 字节长度 + 内容）。
void CloudBackend::appendMqttString(QByteArray &target, const QByteArray &value)
{
    target.append(char((value.size() >> 8) & 0xff)); // 长度高字节
    target.append(char(value.size() & 0xff)); // 长度低字节
    target.append(value); // 字符串内容
}

// 更新状态文本，仅在变化时通知 QML。
void CloudBackend::setStatus(const QString &status)
{
    if (m_status == status) // 状态未变化则跳过
        return;
    m_status = status; // 更新状态
    qInfo() << "Cloud:" << status;
    emit stateChanged(); // 通知状态变更
}