#include "backend.h"

#include <QFile>
#include <QHostAddress>
#include <QJsonParseError>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTimer>
#include <QtMath>

namespace {
constexpr quint16 kGpsPort = 8080;

bool isUsableIpv4(const QHostAddress &address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol
        || address.isLoopback()
        || address.isNull()) {
        return false;
    }

    const QString text = address.toString();
    return !text.startsWith("169.254.");
}
}

Backend::Backend(QObject *parent)
    : QObject(parent),
      server(new QTcpServer(this)),
      m_phonePresenceTimer(new QTimer(this))
{
    m_phonePresenceTimer->setSingleShot(true);
    m_phonePresenceTimer->setInterval(8000);
    connect(m_phonePresenceTimer, &QTimer::timeout, this, [this]() {
        if (!m_phoneConnected)
            return;
        m_phoneConnected = false;
        qInfo() << "Phone device lease expired; STM32 detection may resume";
        emit phoneConnectionChanged();
    });

    connect(server,
            &QTcpServer::newConnection,
            this,
            &Backend::newConnection);

    m_serverRunning = server->listen(QHostAddress::AnyIPv4, kGpsPort);
    if (m_serverRunning) {
        const QString hotspotAddress = preferredHotspotAddress();
        m_accessUrl = QStringLiteral("http://%1:%2/")
                          .arg(hotspotAddress)
                          .arg(kGpsPort);
        qDebug() << "GPS web server:" << m_accessUrl;
    } else {
        m_accessUrl = QStringLiteral("Port 8080 unavailable: %1")
                          .arg(server->errorString());
        qWarning() << m_accessUrl;
    }
}

double Backend::latitude() const { return m_latitude; }
double Backend::longitude() const { return m_longitude; }
double Backend::height() const { return m_height; }
double Backend::speed() const { return m_speed; }
double Backend::battery() const { return m_battery; }
double Backend::pitch() const { return m_pitch; }
double Backend::roll() const { return m_roll; }
double Backend::yaw() const { return m_yaw; }
double Backend::accuracy() const { return m_accuracy; }
QString Backend::gpsTime() const { return m_gpsTime; }
QString Backend::accessUrl() const { return m_accessUrl; }
bool Backend::serverRunning() const { return m_serverRunning; }
bool Backend::phoneConnected() const { return m_phoneConnected; }

void Backend::markPhoneActivity()
{
    m_phonePresenceTimer->start();
    if (m_phoneConnected)
        return;

    m_phoneConnected = true;
    qInfo() << "Phone device connected; pausing STM32 detection";
    emit phoneConnectionChanged();
}

QString Backend::preferredHotspotAddress() const
{
    QString wifiAddress;
    QString ethernetAddress;
    QString fallbackAddress;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QString interfaceName =
            (interface.humanReadableName() + " " + interface.name()).toLower();
        const bool isVmware = interfaceName.contains("vmware")
                              || interfaceName.contains("virtualbox");

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (!isUsableIpv4(address)) {
                continue;
            }

            const QString text = address.toString();

            // Windows 自建移动热点通常使用 192.168.137.0/24。
            if (text.startsWith("192.168.137.")) {
                return text;
            }

            if (!isVmware && interface.type() == QNetworkInterface::Wifi
                && wifiAddress.isEmpty()) {
                wifiAddress = text;
            } else if (!isVmware
                       && interface.type() == QNetworkInterface::Ethernet
                       && ethernetAddress.isEmpty()) {
                ethernetAddress = text;
            } else if (!isVmware && fallbackAddress.isEmpty()) {
                fallbackAddress = text;
            }
        }
    }

    if (!wifiAddress.isEmpty()) {
        return wifiAddress;
    }
    if (!ethernetAddress.isEmpty()) {
        return ethernetAddress;
    }
    if (!fallbackAddress.isEmpty()) {
        return fallbackAddress;
    }
    return QStringLiteral("127.0.0.1");
}

void Backend::newConnection()
{
    while (server->hasPendingConnections()) {
        QTcpSocket *socket = server->nextPendingConnection();
        socket->setProperty("requestBuffer", QByteArray());

        connect(socket,
                &QTcpSocket::readyRead,
                this,
                &Backend::readData);
        connect(socket,
                &QTcpSocket::disconnected,
                socket,
                &QObject::deleteLater);
    }
}

void Backend::sendResponse(QTcpSocket *socket,
                           int statusCode,
                           const QByteArray &statusText,
                           const QByteArray &contentType,
                           const QByteArray &body) const
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " "
                + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    socket->write(response);
    socket->disconnectFromHost();
}

void Backend::readData()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    QByteArray request = socket->property("requestBuffer").toByteArray();
    request += socket->readAll();
    socket->setProperty("requestBuffer", request);

    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }

    const QByteArray headers = request.left(headerEnd);
    const QList<QByteArray> headerLines = headers.split('\n');
    if (headerLines.isEmpty()) {
        return;
    }

    const QByteArray requestLine = headerLines.first().trimmed();
    int contentLength = 0;
    for (const QByteArray &rawLine : headerLines) {
        const QByteArray line = rawLine.trimmed();
        if (line.toLower().startsWith("content-length:")) {
            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            break;
        }
    }

    const int bodyStart = headerEnd + 4;
    if (request.size() < bodyStart + contentLength) {
        return;
    }

    const QByteArray body = request.mid(bodyStart, contentLength);

    if (requestLine.startsWith("OPTIONS ")) {
        sendResponse(socket, 204, "No Content", "text/plain", QByteArray());
        return;
    }

    if (requestLine.startsWith("GET / ")
        || requestLine.startsWith("GET /index.html ")) {
        markPhoneActivity();
        QFile page(QStringLiteral(":/index.html"));
        if (!page.open(QIODevice::ReadOnly)) {
            sendResponse(socket,
                         500,
                         "Internal Server Error",
                         "text/plain; charset=utf-8",
                         "GPS page resource not found");
            return;
        }

        sendResponse(socket,
                     200,
                     "OK",
                     "text/html; charset=utf-8",
                     page.readAll());
        return;
    }

    if (requestLine.startsWith("GET /health ")) {
        markPhoneActivity();
        const QByteArray health =
            QByteArray("{\"ok\":true,\"url\":\"")
            + m_accessUrl.toUtf8()
            + QByteArray("\"}");
        sendResponse(socket, 200, "OK", "application/json", health);
        return;
    }

    if (requestLine.startsWith("POST /gps ")) {
        if (!processGpsPayload(body)) {
            sendResponse(socket,
                         400,
                         "Bad Request",
                         "application/json",
                         "{\"ok\":false,\"error\":\"invalid gps json\"}");
            return;
        }

        markPhoneActivity();

        sendResponse(socket,
                     200,
                     "OK",
                     "application/json",
                     "{\"ok\":true}");
        return;
    }

    if (requestLine.startsWith("POST /sensor ")) {
        if (!processSensorPayload(body)) {
            sendResponse(socket,
                         400,
                         "Bad Request",
                         "application/json",
                         "{\"ok\":false,\"error\":\"invalid sensor json\"}");
            return;
        }

        markPhoneActivity();

        sendResponse(socket,
                     200,
                     "OK",
                     "application/json",
                     "{\"ok\":true}");
        return;
    }

    sendResponse(socket, 404, "Not Found", "text/plain", "Not found");
}

bool Backend::processGpsPayload(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Invalid GPS JSON:" << error.errorString();
        return false;
    }

    const QJsonObject object = document.object();
    double lat = object.value("latitude").toDouble();
    double lon = object.value("longitude").toDouble();
    const double alt = object.value("height").toDouble();
    const double batteryValue = object.value("battery").toDouble();
    const double pitchValue = object.value("pitch").toDouble();
    const double rollValue = object.value("roll").toDouble();
    const double yawValue = object.value("yaw").toDouble();
    const double accuracyValue = object.value("accuracy").toDouble(0);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    const bool coordinateValid = qIsFinite(lat)
                                 && qIsFinite(lon)
                                 && lat >= -90.0
                                 && lat <= 90.0
                                 && lon >= -180.0
                                 && lon <= 180.0
                                 && !(qAbs(lat) < 0.000001
                                      && qAbs(lon) < 0.000001);
    const bool accuracyAcceptable =
        accuracyValue <= 0.0 || accuracyValue <= 100.0;
    bool positionAccepted = coordinateValid && accuracyAcceptable;

    if (!positionAccepted) {
        lat = m_latitude;
        lon = m_longitude;
        m_speed = 0;
    } else if (lastTime != 0) {
        const double elapsedSeconds = (now - lastTime) / 1000.0;
        const double deltaLat = (lat - lastLat) * 111000.0;
        const double deltaLon = (lon - lastLon)
                                * 111000.0
                                * qCos(qDegreesToRadians(lat));
        const double distance = qSqrt(deltaLat * deltaLat
                                      + deltaLon * deltaLon);
        const double deadZone =
            qBound(3.0,
                   accuracyValue > 0.0 ? accuracyValue * 0.35 : 3.0,
                   12.0);

        if (distance < deadZone) {
            lat = lastLat;
            lon = lastLon;
            m_speed = 0;
        } else if (elapsedSeconds > 0) {
            const double calculatedSpeed = distance / elapsedSeconds;
            const double jumpLimit = qMax(300.0, accuracyValue * 4.0);
            if (distance > jumpLimit && calculatedSpeed > 120.0) {
                lat = lastLat;
                lon = lastLon;
                m_speed = 0;
                positionAccepted = false;
            } else {
                m_speed = calculatedSpeed;
            }
        }
    }

    if (positionAccepted) {
        lastLat = lat;
        lastLon = lon;
        lastTime = now;
    }

    m_latitude = lat;
    m_longitude = lon;
    m_height = alt;
    m_battery = batteryValue;
    m_pitch = pitchValue;
    m_roll = rollValue;
    m_yaw = yawValue;
    m_accuracy = accuracyValue;
    m_gpsTime = QDateTime::currentDateTime().toString("hh:mm:ss");

    emit gpsChanged();
    return true;
}

bool Backend::processSensorPayload(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    const QJsonObject object = document.object();
    if (!object.contains("temperature") || !object.contains("humidity")) {
        return false;
    }

    const double temperatureValue = object.value("temperature").toDouble(qQNaN());
    const double humidityValue = object.value("humidity").toDouble(qQNaN());
    if (!qIsFinite(temperatureValue)
        || !qIsFinite(humidityValue)
        || temperatureValue < -50.0
        || temperatureValue > 100.0
        || humidityValue < 0.0
        || humidityValue > 100.0) {
        return false;
    }

    emit sensorDataReceived(temperatureValue, humidityValue);
    return true;
}
