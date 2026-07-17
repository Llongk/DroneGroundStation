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

//! 判断地址是否为可用于局域网通信的非回环 IPv4。
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
    // 手机租约到期后释放设备占用并允许 STM32 恢复检测。
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

double Backend::latitude() const { return m_latitude; } // 返回最近一次有效纬度
double Backend::longitude() const { return m_longitude; } // 返回最近一次有效经度
double Backend::height() const { return m_height; } // 返回手机上报的海拔高度
double Backend::speed() const { return m_speed; } // 返回根据连续定位点计算的移动速度
double Backend::battery() const { return m_battery; } // 返回手机上报的电量百分比
double Backend::pitch() const { return m_pitch; } // 返回手机上报的俯仰角
double Backend::roll() const { return m_roll; } // 返回手机上报的横滚角
double Backend::yaw() const { return m_yaw; } // 返回手机上报的偏航角
double Backend::accuracy() const { return m_accuracy; } // 返回手机定位精度（米）
QString Backend::gpsTime() const { return m_gpsTime; } // 返回最近一次 GPS 更新的本地时间
QString Backend::accessUrl() const { return m_accessUrl; } // 返回手机浏览器应访问的 HTTP 地址
bool Backend::serverRunning() const { return m_serverRunning; } // 返回手机 HTTP 服务是否监听成功
bool Backend::phoneConnected() const { return m_phoneConnected; } // 返回手机是否处于活动租约内

// 建立或刷新手机设备的八秒活动租约，暂停 STM32 检测。
void Backend::markPhoneActivity()
{
    m_phonePresenceTimer->start();
    if (m_phoneConnected)
        return;

    m_phoneConnected = true;
    qInfo() << "Phone device connected; pausing STM32 detection";
    emit phoneConnectionChanged();
}

// 从活动网卡中选择最适合手机访问的 IPv4 地址：
// 优先返回 Windows 移动热点子网（192.168.137.x），其次 WiFi、以太网、其他地址。
QString Backend::preferredHotspotAddress() const
{
    QString wifiAddress; // 优先 WiFi 地址
    QString ethernetAddress; // 次选以太网地址
    QString fallbackAddress; // 兜底地址

    const auto interfaces = QNetworkInterface::allInterfaces(); // 获取所有网络接口
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags(); // 获取网卡状态标志
        if (!flags.testFlag(QNetworkInterface::IsUp) // 跳过未启用的网卡
            || !flags.testFlag(QNetworkInterface::IsRunning) // 跳过未运行的网卡
            || flags.testFlag(QNetworkInterface::IsLoopBack)) { // 跳过回环接口
            continue;
        }

        const QString interfaceName =
            (interface.humanReadableName() + " " + interface.name()).toLower(); // 合并接口名用于识别
        const bool isVmware = interfaceName.contains("vmware") // 排除 VMware 虚拟网卡
                              || interfaceName.contains("virtualbox"); // 排除 VirtualBox 虚拟网卡

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip(); // 获取该网卡的 IP 地址
            if (!isUsableIpv4(address)) { // 跳过不可用的 IPv4 地址
                continue;
            }

            const QString text = address.toString(); // 转为字符串以便前缀匹配

            // Windows 自建移动热点通常使用 192.168.137.0/24。
            if (text.startsWith("192.168.137.")) {
                return text; // 移动热点地址优先级最高，直接返回
            }

            if (!isVmware && interface.type() == QNetworkInterface::Wifi
                && wifiAddress.isEmpty()) {
                wifiAddress = text; // 记录第一个 WiFi 地址
            } else if (!isVmware
                       && interface.type() == QNetworkInterface::Ethernet
                       && ethernetAddress.isEmpty()) {
                ethernetAddress = text; // 记录第一个以太网地址
            } else if (!isVmware && fallbackAddress.isEmpty()) {
                fallbackAddress = text; // 记录第一个兜底地址
            }
        }
    }

    if (!wifiAddress.isEmpty()) {
        return wifiAddress; // 优先返回 WiFi 地址
    }
    if (!ethernetAddress.isEmpty()) {
        return ethernetAddress; // 次选以太网地址
    }
    if (!fallbackAddress.isEmpty()) {
        return fallbackAddress; // 最后返回兜底地址
    }
    return QStringLiteral("127.0.0.1"); // 无可用地址时回退到本地回环
}

// 接收并配置新的手机 HTTP TCP 连接，为每个连接绑定请求缓冲区。
void Backend::newConnection()
{
    while (server->hasPendingConnections()) { // 处理所有待处理的连接
        QTcpSocket *socket = server->nextPendingConnection(); // 获取下一个客户端 socket
        socket->setProperty("requestBuffer", QByteArray()); // 初始化请求缓冲区

        connect(socket,
                &QTcpSocket::readyRead,
                this,
                &Backend::readData); // 有数据到达时读取并解析 HTTP 请求
        connect(socket,
                &QTcpSocket::disconnected,
                socket,
                &QObject::deleteLater); // 断开时自动释放 socket
    }
}

// 向客户端发送带 CORS 和长度信息的完整 HTTP 响应。
void Backend::sendResponse(QTcpSocket *socket,
                           int statusCode,
                           const QByteArray &statusText,
                           const QByteArray &contentType,
                           const QByteArray &body) const
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " // 状态行
                + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n"; // 内容类型头
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n"; // 内容长度头
    response += "Access-Control-Allow-Origin: *\r\n"; // 允许跨域访问
    response += "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"; // 允许的 HTTP 方法
    response += "Access-Control-Allow-Headers: Content-Type\r\n"; // 允许的请求头
    response += "Cache-Control: no-store\r\n"; // 禁止缓存
    response += "Connection: close\r\n\r\n"; // 短连接
    response += body; // 响应体

    socket->write(response); // 发送响应
    socket->disconnectFromHost(); // 发送完毕后断开连接
}

// 组装、解析并路由一条手机 HTTP 请求。
// 支持 GET / (GPS 页面)、GET /health (健康检查)、POST /gps (GPS 数据)、POST /sensor (温湿度)。
void Backend::readData()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender()); // 获取发送信号的 socket
    if (!socket) {
        return; // 发送者不是 QTcpSocket，忽略
    }

    QByteArray request = socket->property("requestBuffer").toByteArray(); // 读取累积的请求缓冲区
    request += socket->readAll(); // 追加新到达的数据
    socket->setProperty("requestBuffer", request); // 更新缓冲区

    const int headerEnd = request.indexOf("\r\n\r\n"); // 查找 HTTP 头结束位置
    if (headerEnd < 0) {
        return; // 头部尚未完整接收，等待更多数据
    }

    const QByteArray headers = request.left(headerEnd); // 提取 HTTP 头部
    const QList<QByteArray> headerLines = headers.split('\n'); // 逐行分割头部
    if (headerLines.isEmpty()) {
        return; // 空头部，忽略
    }

    const QByteArray requestLine = headerLines.first().trimmed(); // 获取请求行（如 GET / HTTP/1.1）
    int contentLength = 0; // 初始化 Content-Length
    for (const QByteArray &rawLine : headerLines) {
        const QByteArray line = rawLine.trimmed();
        if (line.toLower().startsWith("content-length:")) { // 查找 Content-Length 头
            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt(); // 提取长度值
            break;
        }
    }

    const int bodyStart = headerEnd + 4; // 计算请求体起始位置（跳过 \r\n\r\n）
    if (request.size() < bodyStart + contentLength) { // 请求体尚未完整接收
        return;
    }

    const QByteArray body = request.mid(bodyStart, contentLength); // 提取请求体

    // 处理 CORS 预检请求
    if (requestLine.startsWith("OPTIONS ")) {
        sendResponse(socket, 204, "No Content", "text/plain", QByteArray()); // 返回 204 无内容
        return;
    }

    // 处理 GPS 页面或首页请求
    if (requestLine.startsWith("GET / ")
        || requestLine.startsWith("GET /index.html ")) {
        markPhoneActivity(); // 刷新手机活动租约
        QFile page(QStringLiteral(":/index.html")); // 从 Qt 资源加载 HTML 页面
        if (!page.open(QIODevice::ReadOnly)) { // 打开失败
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
                     page.readAll()); // 返回 HTML 页面内容
        return;
    }

    // 处理健康检查请求
    if (requestLine.startsWith("GET /health ")) {
        markPhoneActivity(); // 刷新手机活动租约
        const QByteArray health =
            QByteArray("{\"ok\":true,\"url\":\"")
            + m_accessUrl.toUtf8()
            + QByteArray("\"}"); // 构造健康检查 JSON 响应
        sendResponse(socket, 200, "OK", "application/json", health);
        return;
    }

    // 处理 GPS 数据上报
    if (requestLine.startsWith("POST /gps ")) {
        if (!processGpsPayload(body)) { // 解析 GPS JSON 失败
            sendResponse(socket,
                         400,
                         "Bad Request",
                         "application/json",
                         "{\"ok\":false,\"error\":\"invalid gps json\"}");
            return;
        }

        markPhoneActivity(); // 刷新手机活动租约

        sendResponse(socket,
                     200,
                     "OK",
                     "application/json",
                     "{\"ok\":true}"); // 返回成功响应
        return;
    }

    // 处理传感器数据上报
    if (requestLine.startsWith("POST /sensor ")) {
        if (!processSensorPayload(body)) { // 解析传感器 JSON 失败
            sendResponse(socket,
                         400,
                         "Bad Request",
                         "application/json",
                         "{\"ok\":false,\"error\":\"invalid sensor json\"}");
            return;
        }

        markPhoneActivity(); // 刷新手机活动租约

        sendResponse(socket,
                     200,
                     "OK",
                     "application/json",
                     "{\"ok\":true}"); // 返回成功响应
        return;
    }

    // 未匹配任何路由，返回 404
    sendResponse(socket, 404, "Not Found", "text/plain", "Not found");
}

// 校验手机 GPS JSON，并完成防漂移和异常跳点过滤。
// 包含坐标合法性检查、精度过滤、静止死区、速度跳变检测等逻辑。
bool Backend::processGpsPayload(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error); // 解析 JSON
    if (error.error != QJsonParseError::NoError || !document.isObject()) { // JSON 无效
        qWarning() << "Invalid GPS JSON:" << error.errorString();
        return false;
    }

    const QJsonObject object = document.object(); // 获取 JSON 对象
    double lat = object.value("latitude").toDouble(); // 读取纬度
    double lon = object.value("longitude").toDouble(); // 读取经度
    const double alt = object.value("height").toDouble(); // 读取海拔高度
    const double batteryValue = object.value("battery").toDouble(); // 读取电量
    const double pitchValue = object.value("pitch").toDouble(); // 读取俯仰角
    const double rollValue = object.value("roll").toDouble(); // 读取横滚角
    const double yawValue = object.value("yaw").toDouble(); // 读取偏航角
    const double accuracyValue = object.value("accuracy").toDouble(0); // 读取定位精度（默认 0）
    const qint64 now = QDateTime::currentMSecsSinceEpoch(); // 获取当前时间戳

    const bool coordinateValid = qIsFinite(lat) // 纬度必须有限
                                 && qIsFinite(lon) // 经度必须有限
                                 && lat >= -90.0 // 纬度范围 [-90, 90]
                                 && lat <= 90.0
                                 && lon >= -180.0 // 经度范围 [-180, 180]
                                 && lon <= 180.0
                                 && !(qAbs(lat) < 0.000001 // 排除 (0,0) 无效坐标
                                      && qAbs(lon) < 0.000001);
    const bool accuracyAcceptable =
        accuracyValue <= 0.0 || accuracyValue <= 100.0; // 精度未知或 ≤100m 视为可接受
    bool positionAccepted = coordinateValid && accuracyAcceptable; // 综合判断定位是否可用

    if (!positionAccepted) { // 定位不可用时保持上次位置，速度清零
        lat = m_latitude;
        lon = m_longitude;
        m_speed = 0;
    } else if (lastTime != 0) { // 有历史定位点时进行防漂移和跳点检测
        const double elapsedSeconds = (now - lastTime) / 1000.0; // 计算时间间隔（秒）
        const double deltaLat = (lat - lastLat) * 111000.0; // 纬度差转米（1°≈111km）
        const double deltaLon = (lon - lastLon)
                                * 111000.0
                                * qCos(qDegreesToRadians(lat)); // 经度差转米（需纬度修正）
        const double distance = qSqrt(deltaLat * deltaLat
                                      + deltaLon * deltaLon); // 欧几里得距离（米）
        const double deadZone =
            qBound(3.0, // 死区下限 3m
                   accuracyValue > 0.0 ? accuracyValue * 0.35 : 3.0, // 根据精度动态计算死区
                   12.0); // 死区上限 12m

        if (distance < deadZone) { // 位移小于死区，视为静止漂移
            lat = lastLat; // 保持上次纬度
            lon = lastLon; // 保持上次经度
            m_speed = 0; // 速度清零
        } else if (elapsedSeconds > 0) { // 计算实际速度
            const double calculatedSpeed = distance / elapsedSeconds; // 计算速度（m/s）
            const double jumpLimit = qMax(300.0, accuracyValue * 4.0); // 跳点检测阈值
            if (distance > jumpLimit && calculatedSpeed > 120.0) { // 超过阈值且速度异常
                lat = lastLat; // 拒绝跳点，保持上次位置
                lon = lastLon;
                m_speed = 0;
                positionAccepted = false; // 标记为异常跳点
            } else {
                m_speed = calculatedSpeed; // 更新移动速度
            }
        }
    }

    if (positionAccepted) { // 定位有效时更新历史记录
        lastLat = lat; // 保存上次有效纬度
        lastLon = lon; // 保存上次有效经度
        lastTime = now; // 保存上次更新时间
    }

    m_latitude = lat; // 更新当前纬度
    m_longitude = lon; // 更新当前经度
    m_height = alt; // 更新当前高度
    m_battery = batteryValue; // 更新当前电量
    m_pitch = pitchValue; // 更新当前俯仰角
    m_roll = rollValue; // 更新当前横滚角
    m_yaw = yawValue; // 更新当前偏航角
    m_accuracy = accuracyValue; // 更新当前精度
    m_gpsTime = QDateTime::currentDateTime().toString("hh:mm:ss"); // 更新 GPS 时间显示

    emit gpsChanged(); // 通知 QML 界面 GPS 数据已更新
    return true;
}

// 校验手机温湿度 JSON 并发出传感器更新信号。
// 检查 temperature 和 humidity 字段是否存在且数值在合理范围内。
bool Backend::processSensorPayload(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error); // 解析 JSON
    if (error.error != QJsonParseError::NoError || !document.isObject()) { // JSON 无效
        return false;
    }

    const QJsonObject object = document.object(); // 获取 JSON 对象
    if (!object.contains("temperature") || !object.contains("humidity")) { // 缺少必要字段
        return false;
    }

    const double temperatureValue = object.value("temperature").toDouble(qQNaN()); // 读取温度
    const double humidityValue = object.value("humidity").toDouble(qQNaN()); // 读取湿度
    if (!qIsFinite(temperatureValue) // 温度必须有限
        || !qIsFinite(humidityValue) // 湿度必须有限
        || temperatureValue < -50.0 // 温度下限 -50℃
        || temperatureValue > 100.0 // 温度上限 100℃
        || humidityValue < 0.0 // 湿度下限 0%
        || humidityValue > 100.0) { // 湿度上限 100%
        return false;
    }

    emit sensorDataReceived(temperatureValue, humidityValue); // 通知传感器数据已更新
    return true;
}