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

//! 生成可用于 MQTT Client ID 的安全字符文本。
QString normalizedClientId(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                  QStringLiteral("-"));
    return value.left(64);
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

bool CloudBackend::enabled() const { return m_enabled; }
bool CloudBackend::connected() const { return m_mqttConnected; }
QString CloudBackend::status() const { return m_status; }
QString CloudBackend::host() const { return m_host; }
int CloudBackend::port() const { return m_port; }
QString CloudBackend::username() const { return m_username; }
QString CloudBackend::clientId() const { return m_clientId; }
int CloudBackend::pendingCount() const { return m_pendingCount; }

bool CloudBackend::openOutbox()
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_historyDatabase->databasePath());
    if (!m_database.open()) {
        setStatus(QStringLiteral("无法打开云端离线队列：%1")
                      .arg(m_database.lastError().text()));
        return false;
    }
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
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
    )SQL"))) {
        setStatus(QStringLiteral("无法创建云端离线队列：%1").arg(query.lastError().text()));
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cloud_pending ON cloud_outbox(uploaded, id)"));
    bool hasHttpUploaded = false;
    if (query.exec(QStringLiteral("PRAGMA table_info(cloud_outbox)"))) {
        while (query.next()) {
            if (query.value(1).toString() == QStringLiteral("http_uploaded")) {
                hasHttpUploaded = true;
                break;
            }
        }
    }
    if (!hasHttpUploaded
        && !query.exec(QStringLiteral(
            "ALTER TABLE cloud_outbox ADD COLUMN http_uploaded INTEGER NOT NULL DEFAULT 0"))) {
        setStatus(QStringLiteral("无法升级云端历史队列：%1").arg(query.lastError().text()));
        return false;
    }
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_cloud_http_pending ON cloud_outbox(http_uploaded, id)"));

    // After a restart, expose the newest persisted telemetry immediately so
    // the cloud dashboard is useful while the older backlog is still draining.
    if (query.exec(QStringLiteral(
            "SELECT id FROM cloud_outbox "
            "WHERE http_uploaded=0 ORDER BY created_ms DESC, id DESC LIMIT 1"))
        && query.next()) {
        m_httpPriorityRowId = query.value(0).toLongLong();
    }
    updatePendingCount();
    return true;
}

QString CloudBackend::configurationDirectory() const
{
    QDir databaseDirectory(QFileInfo(m_historyDatabase->databasePath()).absolutePath());
    databaseDirectory.cdUp();
    return databaseDirectory.filePath(QStringLiteral("config"));
}

void CloudBackend::loadConfiguration()
{
    QFile file(QDir(configurationDirectory()).filePath(QStringLiteral("cloud.json")));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    m_host = object.value(QStringLiteral("host")).toString(m_host).trimmed();
    m_port = static_cast<quint16>(object.value(QStringLiteral("port")).toInt(m_port));
    m_username = object.value(QStringLiteral("username")).toString(m_username);
    m_password = object.value(QStringLiteral("password")).toString();
    m_clientId = normalizedClientId(object.value(QStringLiteral("clientId")).toString(m_clientId));
    m_historyIngestUrl = object.value(QStringLiteral("historyIngestUrl"))
        .toString(m_historyIngestUrl).trimmed();
    m_historyIngestKey = object.value(QStringLiteral("historyIngestKey")).toString().trimmed();
    m_enabled = object.value(QStringLiteral("enabled")).toBool(false) && !m_password.isEmpty();
    if (m_enabled)
        m_status = QStringLiteral("等待连接 EMQX Cloud");
}

bool CloudBackend::saveConfiguration() const
{
    QDir directory(configurationDirectory());
    if (!directory.mkpath(QStringLiteral(".")))
        return false;
    QJsonObject object;
    object.insert(QStringLiteral("host"), m_host);
    object.insert(QStringLiteral("port"), m_port);
    object.insert(QStringLiteral("username"), m_username);
    object.insert(QStringLiteral("password"), m_password);
    object.insert(QStringLiteral("clientId"), m_clientId);
    object.insert(QStringLiteral("historyIngestUrl"), m_historyIngestUrl);
    object.insert(QStringLiteral("historyIngestKey"), m_historyIngestKey);
    object.insert(QStringLiteral("enabled"), m_enabled);
    QFile file(directory.filePath(QStringLiteral("cloud.json")));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) > 0;
}

bool CloudBackend::configure(const QString &host,
                             int port,
                             const QString &username,
                             const QString &password,
                             const QString &clientId,
                             bool enabled)
{
    if (host.trimmed().isEmpty() || port < 1 || port > 65535
        || username.trimmed().isEmpty() || clientId.trimmed().isEmpty()) {
        setStatus(QStringLiteral("云端配置不完整"));
        return false;
    }
    if (enabled && password.isEmpty() && m_password.isEmpty()) {
        setStatus(QStringLiteral("请输入 EMQX 认证密码"));
        return false;
    }
    m_host = host.trimmed();
    m_port = static_cast<quint16>(port);
    m_username = username.trimmed();
    if (!password.isEmpty())
        m_password = password;
    m_clientId = normalizedClientId(clientId.trimmed());
    m_enabled = enabled;
    emit configurationChanged();
    emit stateChanged();
    if (!saveConfiguration()) {
        setStatus(QStringLiteral("无法保存 config/cloud.json"));
        return false;
    }
    reconnect();
    return true;
}

void CloudBackend::reconnect()
{
    m_reconnectTimer->stop();
    m_keepAliveTimer->stop();
    m_mqttConnected = false;
    m_inFlight.clear();
    m_socket->abort();
    emit stateChanged();
    if (m_enabled)
        QTimer::singleShot(0, this, &CloudBackend::connectToCloud);
    else
        setStatus(QStringLiteral("云端上传已关闭"));
}

void CloudBackend::connectToCloud()
{
    if (!m_enabled || m_host.isEmpty() || m_password.isEmpty())
        return;
    setStatus(QStringLiteral("正在通过 TLS 连接 %1:%2").arg(m_host).arg(m_port));
    const QString caPath = QDir(configurationDirectory()).filePath(QStringLiteral("emqxsl-ca.crt"));
    const QList<QSslCertificate> certificates = QSslCertificate::fromPath(caPath);
    if (certificates.isEmpty()) {
        setStatus(QStringLiteral("未找到有效 CA：%1").arg(caPath));
        scheduleReconnect();
        return;
    }
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    QList<QSslCertificate> authorities = ssl.caCertificates();
    authorities.append(certificates);
    ssl.setCaCertificates(authorities);
    m_socket->setSslConfiguration(ssl);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->setPeerVerifyName(m_host);
    m_socket->connectToHostEncrypted(m_host, m_port);
}

void CloudBackend::tlsEncrypted()
{
    setStatus(QStringLiteral("TLS 已建立，正在进行 MQTT 认证"));
    sendConnectPacket();
}

void CloudBackend::sendConnectPacket()
{
    QByteArray body;
    appendMqttString(body, QByteArrayLiteral("MQTT"));
    body.append(char(4));
    body.append(char(0xC2)); // username + password + clean session
    body.append(char((kKeepAliveSeconds >> 8) & 0xff));
    body.append(char(kKeepAliveSeconds & 0xff));
    appendMqttString(body, m_clientId.toUtf8());
    appendMqttString(body, m_username.toUtf8());
    appendMqttString(body, m_password.toUtf8());
    m_socket->write(mqttPacket(0x10, body));
}

void CloudBackend::socketReadyRead()
{
    m_buffer.append(m_socket->readAll());
    if (m_buffer.size() > kMaximumPacketSize) {
        setStatus(QStringLiteral("云端 MQTT 数据包超过安全上限"));
        m_socket->abort();
        return;
    }
    processPackets();
}

void CloudBackend::processPackets()
{
    while (m_buffer.size() >= 2) {
        const quint8 header = static_cast<quint8>(m_buffer.at(0));
        int multiplier = 1;
        int remaining = 0;
        int index = 1;
        quint8 encoded = 0;
        do {
            if (index >= m_buffer.size())
                return;
            encoded = static_cast<quint8>(m_buffer.at(index++));
            remaining += (encoded & 127) * multiplier;
            multiplier *= 128;
            if (multiplier > 128 * 128 * 128 * 128) {
                m_socket->abort();
                return;
            }
        } while (encoded & 128);
        if (m_buffer.size() < index + remaining)
            return;
        const QByteArray body = m_buffer.mid(index, remaining);
        m_buffer.remove(0, index + remaining);
        processPacket(header, body);
    }
}

void CloudBackend::processPacket(quint8 header, const QByteArray &body)
{
    const int type = header >> 4;
    if (type == 2) { // CONNACK
        if (body.size() < 2 || static_cast<quint8>(body.at(1)) != 0) {
            const int code = body.size() >= 2 ? static_cast<quint8>(body.at(1)) : -1;
            setStatus(QStringLiteral("EMQX MQTT 认证失败，返回码 %1").arg(code));
            m_socket->disconnectFromHost();
            return;
        }
        m_mqttConnected = true;
        m_keepAliveTimer->start();
        setStatus(QStringLiteral("EMQX 云端已连接"));
        emit stateChanged();
        pumpOutbox();
        return;
    }
    if (type == 4 && body.size() >= 2) { // PUBACK
        const quint16 id = (static_cast<quint8>(body.at(0)) << 8)
            | static_cast<quint8>(body.at(1));
        if (m_inFlight.contains(id))
            markUploaded(m_inFlight.take(id));
        pumpOutbox();
        return;
    }
    if (type == 13)
        return; // PINGRESP
}

void CloudBackend::socketDisconnected()
{
    const bool wasConnected = m_mqttConnected;
    m_mqttConnected = false;
    m_keepAliveTimer->stop();
    m_inFlight.clear();
    if (wasConnected)
        setStatus(QStringLiteral("云端连接已断开，数据将在本地排队"));
    emit stateChanged();
    scheduleReconnect();
}

void CloudBackend::socketError()
{
    if (!m_enabled)
        return;
    setStatus(QStringLiteral("云端连接错误：%1").arg(m_socket->errorString()));
    scheduleReconnect();
}

void CloudBackend::scheduleReconnect()
{
    if (m_enabled && !m_reconnectTimer->isActive())
        m_reconnectTimer->start();
}

void CloudBackend::sendKeepAlive()
{
    if (m_mqttConnected)
        m_socket->write(QByteArray::fromHex("c000"));
}

void CloudBackend::recordPhoneGps() { enqueue(QStringLiteral("dgs/UAV_01/phone/telemetry"), phonePayload(QStringLiteral("gps"))); }
void CloudBackend::recordPhoneSensor() { enqueue(QStringLiteral("dgs/UAV_01/phone/telemetry"), phonePayload(QStringLiteral("sensor"))); }
void CloudBackend::recordStm32Telemetry() { enqueue(QStringLiteral("dgs/UAV_01/stm32/telemetry"), stm32Payload()); }

QByteArray CloudBackend::phonePayload(const QString &eventType) const
{
    QJsonObject object;
    object.insert(QStringLiteral("message_id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    object.insert(QStringLiteral("source"), QStringLiteral("phone"));
    object.insert(QStringLiteral("event_type"), eventType);
    object.insert(QStringLiteral("session_id"), m_historyDatabase->currentSessionId());
    object.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    object.insert(QStringLiteral("latitude"), m_backend->latitude());
    object.insert(QStringLiteral("longitude"), m_backend->longitude());
    object.insert(QStringLiteral("altitude"), m_backend->height());
    object.insert(QStringLiteral("speed"), m_backend->speed());
    object.insert(QStringLiteral("battery"), m_backend->battery());
    object.insert(QStringLiteral("pitch"), m_backend->pitch());
    object.insert(QStringLiteral("roll"), m_backend->roll());
    object.insert(QStringLiteral("yaw"), m_backend->yaw());
    object.insert(QStringLiteral("accuracy"), m_backend->accuracy());
    object.insert(QStringLiteral("temperature"), m_sensorBackend->temperature());
    object.insert(QStringLiteral("humidity"), m_sensorBackend->humidity());
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray CloudBackend::stm32Payload() const
{
    QJsonObject object;
    object.insert(QStringLiteral("message_id"), QStringLiteral("%1-%2-%3")
                      .arg(m_historyDatabase->currentSessionId(),
                           m_sensorBackend->deviceId())
                      .arg(m_sensorBackend->sequence()));
    object.insert(QStringLiteral("source"), QStringLiteral("stm32"));
    object.insert(QStringLiteral("session_id"), m_historyDatabase->currentSessionId());
    object.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
    object.insert(QStringLiteral("device_id"), m_sensorBackend->deviceId());
    object.insert(QStringLiteral("sequence"), static_cast<qint64>(m_sensorBackend->sequence()));
    object.insert(QStringLiteral("latitude"), m_sensorBackend->stm32Latitude());
    object.insert(QStringLiteral("longitude"), m_sensorBackend->stm32Longitude());
    object.insert(QStringLiteral("heading"), m_sensorBackend->heading());
    object.insert(QStringLiteral("target_altitude"), m_sensorBackend->targetAltitude());
    object.insert(QStringLiteral("speed"), m_sensorBackend->stm32Speed());
    object.insert(QStringLiteral("rth_latitude"), m_sensorBackend->rthLatitude());
    object.insert(QStringLiteral("rth_longitude"), m_sensorBackend->rthLongitude());
    object.insert(QStringLiteral("rth_distance"), m_sensorBackend->rthDistance());
    object.insert(QStringLiteral("dht_temperature"), m_sensorBackend->dhtTemperature());
    object.insert(QStringLiteral("dht_humidity"), m_sensorBackend->dhtHumidity());
    object.insert(QStringLiteral("sht_temperature"), m_sensorBackend->shtTemperature());
    object.insert(QStringLiteral("sht_humidity"), m_sensorBackend->shtHumidity());
    object.insert(QStringLiteral("mcu_temperature"), m_sensorBackend->mcuTemperature());
    object.insert(QStringLiteral("flight_mode"), m_sensorBackend->flightMode());
    object.insert(QStringLiteral("alarm_code"), m_sensorBackend->alarmCode());
    object.insert(QStringLiteral("battery"), m_sensorBackend->stm32Battery());
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void CloudBackend::enqueue(const QString &topic, const QByteArray &payload)
{
    if (!m_database.isOpen()
        || m_historyDatabase->currentSessionId().isEmpty())
        return;
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO cloud_outbox(message_id,topic,payload,created_ms) VALUES(?,?,?,?)"));
    query.addBindValue(object.value(QStringLiteral("message_id")).toString());
    query.addBindValue(topic);
    query.addBindValue(QString::fromUtf8(payload));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        qWarning() << "Cannot enqueue cloud message:" << query.lastError().text();
    } else if (query.numRowsAffected() > 0) {
        // Do not make live phone/STM32 telemetry wait behind a large legacy
        // backlog. The next HTTP slot sends the newest row first; after that,
        // the normal oldest-first query continues draining persisted records.
        m_httpPriorityRowId = query.lastInsertId().toLongLong();
    }
    updatePendingCount();
    pumpOutbox();
    pumpHttpOutbox();
}

void CloudBackend::pumpHttpOutbox()
{
    if (m_httpInFlightRowId != 0 || !m_database.isOpen()
        || m_historyIngestUrl.isEmpty() || m_historyIngestKey.isEmpty()) {
        return;
    }

    QSqlQuery query(m_database);
    bool rowAvailable = false;
    if (m_httpPriorityRowId > 0) {
        query.prepare(QStringLiteral(
            "SELECT id,topic,payload FROM cloud_outbox "
            "WHERE id=? AND http_uploaded=0"));
        query.addBindValue(m_httpPriorityRowId);
        rowAvailable = query.exec() && query.next();
        if (!rowAvailable)
            m_httpPriorityRowId = 0;
    }

    if (!rowAvailable) {
        rowAvailable = query.exec(QStringLiteral(
            "SELECT id,topic,payload FROM cloud_outbox "
            "WHERE http_uploaded=0 ORDER BY id LIMIT 1"))
            && query.next();
    }
    if (!rowAvailable)
        return;

    const qint64 rowId = query.value(0).toLongLong();
    const QString topic = query.value(1).toString();
    const QJsonObject payload = QJsonDocument::fromJson(
        query.value(2).toString().toUtf8()).object();
    if (payload.isEmpty()) {
        qWarning() << "Cannot upload empty cloud history payload for row" << rowId;
        return;
    }

    QJsonObject envelope;
    envelope.insert(QStringLiteral("topic"), topic);
    envelope.insert(QStringLiteral("payload"), payload);
    envelope.insert(QStringLiteral("clientid"), m_clientId);
    envelope.insert(QStringLiteral("qos"), 1);
    envelope.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    QUrl url(m_historyIngestUrl);
    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(QStringLiteral("key"), m_historyIngestKey);
    url.setQuery(urlQuery);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_http->post(
        request, QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    reply->setProperty("cloudHistoryRowId", rowId);
    m_httpInFlightRowId = rowId;
}

void CloudBackend::httpFinished(QNetworkReply *reply)
{
    const qint64 rowId = reply->property("cloudHistoryRowId").toLongLong();
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    const bool succeeded = reply->error() == QNetworkReply::NoError
        && statusCode >= 200 && statusCode < 300;

    if (succeeded && rowId > 0) {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "UPDATE cloud_outbox SET http_uploaded=1 WHERE id=?"));
        query.addBindValue(rowId);
        if (!query.exec())
            qWarning() << "Cannot mark D1 history row uploaded:" << query.lastError().text();
        if (rowId == m_httpPriorityRowId)
            m_httpPriorityRowId = 0;
    } else {
        qWarning() << "D1 history upload failed:"
                   << "HTTP" << statusCode
                   << reply->errorString()
                   << responseBody.left(256);
    }

    reply->deleteLater();
    m_httpInFlightRowId = 0;
    if (succeeded)
        QTimer::singleShot(0, this, &CloudBackend::pumpHttpOutbox);
    else if (!m_httpRetryTimer->isActive())
        m_httpRetryTimer->start();
}

void CloudBackend::pumpOutbox()
{
    if (!m_mqttConnected || !m_inFlight.isEmpty() || !m_database.isOpen())
        return;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id,topic,payload FROM cloud_outbox WHERE uploaded=0 ORDER BY id LIMIT 1"))
        || !query.next())
        return;
    const qint64 rowId = query.value(0).toLongLong();
    const QByteArray topic = query.value(1).toString().toUtf8();
    const QByteArray payload = query.value(2).toString().toUtf8();
    const quint16 id = nextPacketId();
    QByteArray body;
    appendMqttString(body, topic);
    body.append(char((id >> 8) & 0xff));
    body.append(char(id & 0xff));
    body.append(payload);
    m_inFlight.insert(id, rowId);
    m_socket->write(mqttPacket(0x32, body)); // QoS 1
    QSqlQuery retry(m_database);
    retry.prepare(QStringLiteral("UPDATE cloud_outbox SET retry_count=retry_count+1 WHERE id=?"));
    retry.addBindValue(rowId);
    retry.exec();
}

void CloudBackend::markUploaded(qint64 rowId)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE cloud_outbox SET uploaded=1 WHERE id=?"));
    query.addBindValue(rowId);
    query.exec();
    updatePendingCount();
}

void CloudBackend::updatePendingCount()
{
    if (!m_database.isOpen())
        return;
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM cloud_outbox WHERE uploaded=0")) && query.next()) {
        const int count = query.value(0).toInt();
        if (count != m_pendingCount) {
            m_pendingCount = count;
            emit pendingCountChanged();
        }
    }
}

quint16 CloudBackend::nextPacketId()
{
    if (++m_packetId == 0)
        ++m_packetId;
    return m_packetId;
}

QByteArray CloudBackend::mqttPacket(quint8 header, const QByteArray &body) const
{
    QByteArray packet;
    packet.append(char(header));
    int length = body.size();
    do {
        quint8 encoded = length % 128;
        length /= 128;
        if (length > 0)
            encoded |= 128;
        packet.append(char(encoded));
    } while (length > 0);
    packet.append(body);
    return packet;
}

void CloudBackend::appendMqttString(QByteArray &target, const QByteArray &value)
{
    target.append(char((value.size() >> 8) & 0xff));
    target.append(char(value.size() & 0xff));
    target.append(value);
}

void CloudBackend::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    qInfo() << "Cloud:" << status;
    emit stateChanged();
}
