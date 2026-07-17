#ifndef CLOUDBACKEND_H
#define CLOUDBACKEND_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSqlDatabase>

class Backend;
class HistoryDatabase;
class QNetworkAccessManager;
class QNetworkReply;
class QSslSocket;
class QTimer;
class SensorBackend;

//! 使用 MQTT 3.1.1/TLS 将本地遥测可靠上传到 EMQX，并维护 SQLite 离线队列。
class CloudBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString host READ host NOTIFY configurationChanged)
    Q_PROPERTY(int port READ port NOTIFY configurationChanged)
    Q_PROPERTY(QString username READ username NOTIFY configurationChanged)
    Q_PROPERTY(QString clientId READ clientId NOTIFY configurationChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    explicit CloudBackend(Backend *backend,
                          SensorBackend *sensorBackend,
                          HistoryDatabase *historyDatabase,
                          QObject *parent = nullptr);
    ~CloudBackend() override;

    bool enabled() const;
    bool connected() const;
    QString status() const;
    QString host() const;
    int port() const;
    QString username() const;
    QString clientId() const;
    int pendingCount() const;

    //! 保存云端连接参数并立即按 enabled 状态重新连接。
    Q_INVOKABLE bool configure(const QString &host,
                               int port,
                               const QString &username,
                               const QString &password,
                               const QString &clientId,
                               bool enabled);
    //! 主动断开后重新建立 MQTT/TLS 连接。
    Q_INVOKABLE void reconnect();

signals:
    void stateChanged();
    void configurationChanged();
    void pendingCountChanged();

private slots:
    void connectToCloud();
    void tlsEncrypted();
    void socketReadyRead();
    void socketDisconnected();
    void socketError();
    void sendKeepAlive();
    void recordPhoneGps();
    void recordPhoneSensor();
    void recordStm32Telemetry();
    void httpFinished(QNetworkReply *reply);

private:
    bool openOutbox();
    void loadConfiguration();
    bool saveConfiguration() const;
    QString configurationDirectory() const;
    void setStatus(const QString &status);
    void scheduleReconnect();
    void sendConnectPacket();
    void processPackets();
    void processPacket(quint8 header, const QByteArray &body);
    void pumpOutbox();
    void pumpHttpOutbox();
    void enqueue(const QString &topic, const QByteArray &payload);
    void updatePendingCount();
    void markUploaded(qint64 rowId);
    quint16 nextPacketId();
    QByteArray mqttPacket(quint8 header, const QByteArray &body) const;
    static void appendMqttString(QByteArray &target, const QByteArray &value);
    QByteArray phonePayload(const QString &eventType) const;
    QByteArray stm32Payload() const;

    Backend *m_backend = nullptr;
    SensorBackend *m_sensorBackend = nullptr;
    HistoryDatabase *m_historyDatabase = nullptr;
    QSslSocket *m_socket = nullptr;
    QNetworkAccessManager *m_http = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_keepAliveTimer = nullptr;
    QTimer *m_httpRetryTimer = nullptr;
    QSqlDatabase m_database;
    QString m_connectionName;
    QByteArray m_buffer;
    QHash<quint16, qint64> m_inFlight;
    QString m_host = QStringLiteral("h00fc100.ala.cn-shenzhen.emqxsl.cn");
    quint16 m_port = 8883;
    QString m_username = QStringLiteral("DroneGroundStation");
    QString m_password;
    QString m_clientId;
    QString m_historyIngestUrl = QStringLiteral(
        "https://dronegroundstation-live.shaniquamatte20348.chatgpt.site/api/ingest");
    QString m_historyIngestKey;
    QString m_status = QStringLiteral("云端尚未配置密码");
    bool m_enabled = false;
    bool m_mqttConnected = false;
    int m_pendingCount = 0;
    qint64 m_httpInFlightRowId = 0;
    qint64 m_httpPriorityRowId = 0;
    quint16 m_packetId = 0;
};

#endif
