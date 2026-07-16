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
    //! 创建手机 HTTP 服务并初始化 GPS 与设备在线状态。
    explicit Backend(QObject *parent = nullptr);

    //! 返回最近一次有效纬度。
    double latitude() const;
    //! 返回最近一次有效经度。
    double longitude() const;
    //! 返回手机上报的海拔高度。
    double height() const;
    //! 返回根据连续定位点计算的移动速度。
    double speed() const;
    //! 返回手机上报的电量百分比。
    double battery() const;
    //! 返回手机上报的俯仰角。
    double pitch() const;
    //! 返回手机上报的横滚角。
    double roll() const;
    //! 返回手机上报的偏航角。
    double yaw() const;
    //! 返回手机定位精度。
    double accuracy() const;
    //! 返回最近一次 GPS 更新的本地时间。
    QString gpsTime() const;
    //! 返回手机浏览器应访问的 HTTP 地址。
    QString accessUrl() const;
    //! 返回手机 HTTP 服务是否监听成功。
    bool serverRunning() const;
    //! 返回手机是否处于活动租约内。
    bool phoneConnected() const;

signals:
    //! GPS、姿态或手机飞行数据发生变化。
    void gpsChanged();
    //! HTTP 服务监听状态或访问地址发生变化。
    void serverStatusChanged();
    //! 收到一组有效手机温湿度。
    void sensorDataReceived(double temperature, double humidity);
    //! 手机活动租约建立或过期。
    void phoneConnectionChanged();

private slots:
    //! 接收并配置新的手机 HTTP TCP 连接。
    void newConnection();
    //! 组装、解析并路由一条手机 HTTP 请求。
    void readData();

private:
    //! 从活动网卡中选择最适合手机访问的 IPv4 地址。
    QString preferredHotspotAddress() const;
    //! 向客户端发送带 CORS 和长度信息的完整 HTTP 响应。
    void sendResponse(QTcpSocket *socket,
                      int statusCode,
                      const QByteArray &statusText,
                      const QByteArray &contentType,
                      const QByteArray &body) const;
    //! 校验手机 GPS JSON，并完成防漂移和异常跳点过滤。
    bool processGpsPayload(const QByteArray &json);
    //! 校验手机温湿度 JSON 并发出传感器更新信号。
    bool processSensorPayload(const QByteArray &json);
    //! 建立或刷新手机设备的八秒活动租约。
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
