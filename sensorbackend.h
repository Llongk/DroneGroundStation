#ifndef SENSORBACKEND_H
#define SENSORBACKEND_H

#include <QByteArray>
#include <QObject>

class QJsonObject;
class QTcpSocket;
class QTimer;

class SensorBackend : public QObject
{
    Q_OBJECT

    // Mobile web sensor values stay independent from STM32 telemetry.
    Q_PROPERTY(double temperature READ temperature NOTIFY sensorChanged)
    Q_PROPERTY(double humidity READ humidity NOTIFY sensorChanged)

    Q_PROPERTY(bool telemetryConnected READ telemetryConnected NOTIFY telemetryConnectionChanged)
    Q_PROPERTY(bool telemetryValid READ telemetryValid NOTIFY telemetryChanged)
    Q_PROPERTY(QString telemetryPeer READ telemetryPeer NOTIFY telemetryConnectionChanged)
    Q_PROPERTY(QString telemetryTarget READ telemetryTarget NOTIFY telemetryConnectionChanged)
    Q_PROPERTY(QString telemetryStatus READ telemetryStatus NOTIFY telemetryConnectionChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY telemetryChanged)
    Q_PROPERTY(int protocolVersion READ protocolVersion NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong sequence READ sequence NOTIFY telemetryChanged)
    Q_PROPERTY(double stm32Latitude READ stm32Latitude NOTIFY telemetryChanged)
    Q_PROPERTY(double stm32Longitude READ stm32Longitude NOTIFY telemetryChanged)
    Q_PROPERTY(double heading READ heading NOTIFY telemetryChanged)
    Q_PROPERTY(double targetAltitude READ targetAltitude NOTIFY telemetryChanged)
    Q_PROPERTY(double stm32Speed READ stm32Speed NOTIFY telemetryChanged)
    Q_PROPERTY(double rthLatitude READ rthLatitude NOTIFY telemetryChanged)
    Q_PROPERTY(double rthLongitude READ rthLongitude NOTIFY telemetryChanged)
    Q_PROPERTY(double rthDistance READ rthDistance NOTIFY telemetryChanged)
    Q_PROPERTY(double dhtTemperature READ dhtTemperature NOTIFY telemetryChanged)
    Q_PROPERTY(double dhtHumidity READ dhtHumidity NOTIFY telemetryChanged)
    Q_PROPERTY(double shtTemperature READ shtTemperature NOTIFY telemetryChanged)
    Q_PROPERTY(double shtHumidity READ shtHumidity NOTIFY telemetryChanged)
    Q_PROPERTY(double mcuTemperature READ mcuTemperature NOTIFY telemetryChanged)
    Q_PROPERTY(int flightMode READ flightMode NOTIFY telemetryChanged)
    Q_PROPERTY(int alarmCode READ alarmCode NOTIFY telemetryChanged)
    Q_PROPERTY(int stm32Battery READ stm32Battery NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong telemetryTimestamp READ telemetryTimestamp NOTIFY telemetryChanged)
    Q_PROPERTY(QString telemetryUpdateTime READ telemetryUpdateTime NOTIFY telemetryChanged)
    Q_PROPERTY(QString commandStatus READ commandStatus NOTIFY commandStateChanged)
    Q_PROPERTY(bool manualControl READ manualControl NOTIFY commandStateChanged)
    Q_PROPERTY(bool commandPending READ commandPending NOTIFY commandStateChanged)
    Q_PROPERTY(qulonglong commandSequence READ commandSequence NOTIFY commandStateChanged)

public:
    explicit SensorBackend(QObject *parent = nullptr);

    double temperature() const;
    double humidity() const;
    bool telemetryConnected() const;
    bool telemetryValid() const;
    QString telemetryPeer() const;
    QString telemetryTarget() const;
    QString telemetryStatus() const;
    QString deviceId() const;
    int protocolVersion() const;
    qulonglong sequence() const;
    double stm32Latitude() const;
    double stm32Longitude() const;
    double heading() const;
    double targetAltitude() const;
    double stm32Speed() const;
    double rthLatitude() const;
    double rthLongitude() const;
    double rthDistance() const;
    double dhtTemperature() const;
    double dhtHumidity() const;
    double shtTemperature() const;
    double shtHumidity() const;
    double mcuTemperature() const;
    int flightMode() const;
    int alarmCode() const;
    int stm32Battery() const;
    qulonglong telemetryTimestamp() const;
    QString telemetryUpdateTime() const;
    QString commandStatus() const;
    bool manualControl() const;
    bool commandPending() const;
    qulonglong commandSequence() const;

    Q_INVOKABLE void reconnectTelemetry();
    Q_INVOKABLE void connectToTelemetry(const QString &host, int port);
    Q_INVOKABLE void useAutomaticTelemetryTarget();
    Q_INVOKABLE bool sendFlightCommand(const QString &command,
                                       double heading,
                                       double targetAltitude,
                                       double speed);

public slots:
    void updateSensorData(double temperature, double humidity);
    void setPhoneDeviceActive(bool active);

signals:
    void sensorChanged();
    void telemetryChanged();
    void telemetryConnectionChanged();
    void commandStateChanged();

private slots:
    void attemptConnection();
    void socketConnected();
    void socketDisconnected();
    void socketError();
    void readTelemetryData();

private:
    QString discoverApServerAddress() const;
    void scheduleReconnect();
    void setConnectionStatus(const QString &status);
    bool processTelemetryFrame(const QByteArray &frame);
    static bool finiteNumber(const QJsonObject &object,
                             const char *key,
                             double *value);

    QTcpSocket *m_socket = nullptr;
    QTimer *m_retryTimer = nullptr;
    QTimer *m_connectTimeout = nullptr;
    QTimer *m_telemetryWatchdog = nullptr;
    QByteArray m_buffer;
    QString m_targetHost;
    quint16 m_targetPort = 8080;
    QString m_telemetryPeer;
    QString m_telemetryStatus = QStringLiteral("正在查找 STM32 热点服务器");
    bool m_automaticTarget = true;
    bool m_phoneDeviceActive = false;

    double m_temperature = 0;
    double m_humidity = 0;
    bool m_telemetryValid = false;
    QString m_deviceId;
    int m_protocolVersion = 0;
    qulonglong m_sequence = 0;
    double m_stm32Latitude = 0;
    double m_stm32Longitude = 0;
    double m_heading = 0;
    double m_targetAltitude = 0;
    double m_stm32Speed = 0;
    double m_rthLatitude = 0;
    double m_rthLongitude = 0;
    double m_rthDistance = 0;
    double m_dhtTemperature = 0;
    double m_dhtHumidity = 0;
    double m_shtTemperature = 0;
    double m_shtHumidity = 0;
    double m_mcuTemperature = 0;
    int m_flightMode = 0;
    int m_alarmCode = 0;
    int m_stm32Battery = 0;
    qulonglong m_telemetryTimestamp = 0;
    QString m_telemetryUpdateTime;
    QString m_commandStatus = QStringLiteral("尚未下发控制命令");
    bool m_manualControl = false;
    bool m_commandPending = false;
    bool m_pendingManualControl = false;
    qulonglong m_pendingCommandSequence = 0;
    qulonglong m_commandSequence = 0;
};

#endif
