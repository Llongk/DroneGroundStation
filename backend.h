#ifndef BACKEND_H
#define BACKEND_H

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class QTimer;

class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double latitude READ latitude NOTIFY gpsChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY gpsChanged)
    Q_PROPERTY(double height READ height NOTIFY gpsChanged)
    Q_PROPERTY(double speed READ speed NOTIFY gpsChanged)
    Q_PROPERTY(double battery READ battery NOTIFY gpsChanged)
    Q_PROPERTY(QString gpsTime READ gpsTime NOTIFY gpsChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY gpsChanged)
    Q_PROPERTY(double roll READ roll NOTIFY gpsChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY gpsChanged)
    Q_PROPERTY(double accuracy READ accuracy NOTIFY gpsChanged)
    Q_PROPERTY(QString accessUrl READ accessUrl NOTIFY serverStatusChanged)
    Q_PROPERTY(bool serverRunning READ serverRunning NOTIFY serverStatusChanged)
    Q_PROPERTY(bool phoneConnected READ phoneConnected NOTIFY phoneConnectionChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    double latitude() const;
    double longitude() const;
    double height() const;
    double speed() const;
    double battery() const;
    double pitch() const;
    double roll() const;
    double yaw() const;
    double accuracy() const;
    QString gpsTime() const;
    QString accessUrl() const;
    bool serverRunning() const;
    bool phoneConnected() const;

signals:
    void gpsChanged();
    void serverStatusChanged();
    void sensorDataReceived(double temperature, double humidity);
    void phoneConnectionChanged();

private slots:
    void newConnection();
    void readData();

private:
    QString preferredHotspotAddress() const;
    void sendResponse(QTcpSocket *socket,
                      int statusCode,
                      const QByteArray &statusText,
                      const QByteArray &contentType,
                      const QByteArray &body) const;
    bool processGpsPayload(const QByteArray &json);
    bool processSensorPayload(const QByteArray &json);
    void markPhoneActivity();

    QTcpServer *server = nullptr;

    double m_latitude = 0;
    double m_longitude = 0;
    double m_height = 0;
    double m_speed = 0;
    double m_battery = 0;
    double m_pitch = 0;
    double m_roll = 0;
    double m_yaw = 0;
    double m_accuracy = 0;
    QString m_gpsTime;

    QString m_accessUrl;
    bool m_serverRunning = false;
    QTimer *m_phonePresenceTimer = nullptr;
    bool m_phoneConnected = false;

    double lastLat = 0;
    double lastLon = 0;
    qint64 lastTime = 0;
};

#endif
