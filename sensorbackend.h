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
    //! 初始化手机传感器状态、STM32 TCP 客户端、重连计时器和看门狗。
    explicit SensorBackend(QObject *parent = nullptr);

    //! 返回手机上报的环境温度。
    double temperature() const;
    //! 返回手机上报的环境湿度。
    double humidity() const;
    //! 返回 STM32 TCP socket 是否已连接。
    bool telemetryConnected() const;
    //! 返回是否已收到并解析至少一帧有效遥测。
    bool telemetryValid() const;
    //! 返回当前 STM32 对端地址。
    QString telemetryPeer() const;
    //! 返回自动发现或手动设置的目标地址。
    QString telemetryTarget() const;
    //! 返回面向界面显示的遥测连接状态。
    QString telemetryStatus() const;
    //! 返回遥测中的设备 ID。
    QString deviceId() const;
    //! 返回遥测协议版本。
    int protocolVersion() const;
    //! 返回最新遥测序号。
    qulonglong sequence() const;
    //! 返回 STM32 当前纬度。
    double stm32Latitude() const;
    //! 返回 STM32 当前经度。
    double stm32Longitude() const;
    //! 返回归一化后的航向角。
    double heading() const;
    //! 返回当前目标高度。
    double targetAltitude() const;
    //! 返回 STM32 飞行速度。
    double stm32Speed() const;
    //! 返回返航点纬度。
    double rthLatitude() const;
    //! 返回返航点经度。
    double rthLongitude() const;
    //! 返回当前位置到返航点的距离。
    double rthDistance() const;
    //! 返回 DHT 温度。
    double dhtTemperature() const;
    //! 返回 DHT 湿度。
    double dhtHumidity() const;
    //! 返回 SHT 温度。
    double shtTemperature() const;
    //! 返回 SHT 湿度。
    double shtHumidity() const;
    //! 返回 MCU 内部温度。
    double mcuTemperature() const;
    //! 返回固件飞行模式代码。
    int flightMode() const;
    //! 返回固件告警代码。
    int alarmCode() const;
    //! 返回 STM32 模拟或实测电量。
    int stm32Battery() const;
    //! 返回设备侧遥测时间戳。
    qulonglong telemetryTimestamp() const;
    //! 返回本机最近一次遥测更新时间。
    QString telemetryUpdateTime() const;
    //! 返回最近一次控制命令状态。
    QString commandStatus() const;
    //! 返回当前是否处于人工控制模式。
    bool manualControl() const;
    //! 返回是否有命令正在等待 STM32 确认。
    bool commandPending() const;
    //! 返回最近生成的命令序号。
    qulonglong commandSequence() const;

    //! 立即中断当前连接并重新连接 STM32。
    Q_INVOKABLE void reconnectTelemetry();
    //! 切换到指定 IPv4 和端口的手动连接目标。
    Q_INVOKABLE void connectToTelemetry(const QString &host, int port);
    //! 恢复根据 WLAN 子网自动推导 STM32 网关。
    Q_INVOKABLE void useAutomaticTelemetryTarget();
    //! 校验参数并向 STM32 下发一条带序号的飞控命令。
    Q_INVOKABLE bool sendFlightCommand(const QString &command,
                                       double heading,
                                       double targetAltitude,
                                       double speed);

public slots:
    //! 保存手机网页上报的温湿度并通知界面。
    void updateSensorData(double temperature, double humidity);
    //! 手机活动时暂停或恢复所有 STM32 检测流程。
    void setPhoneDeviceActive(bool active);

signals:
    //! 手机温湿度发生变化。
    void sensorChanged();
    //! 有效 STM32 遥测内容发生变化。
    void telemetryChanged();
    //! STM32 连接目标或状态发生变化。
    void telemetryConnectionChanged();
    //! 飞控命令等待、模式或结果发生变化。
    void commandStateChanged();

private slots:
    //! 根据当前目标配置尝试建立 STM32 TCP 连接。
    void attemptConnection();
    //! 处理 TCP 建连成功并启动遥测看门狗。
    void socketConnected();
    //! 清理断开的连接、待确认命令和接收缓存。
    void socketDisconnected();
    //! 处理 socket 错误并安排自动重连。
    void socketError();
    //! 从 TCP 缓冲区提取完整的换行分隔 JSON 帧。
    void readTelemetryData();

private:
    //! 根据活动 WLAN 的 IPv4 与掩码推导 AP 网关地址。
    QString discoverApServerAddress() const;
    //! 在未被手机占用时安排下一次重连。
    void scheduleReconnect();
    //! 更新连接状态并仅在发生变化时通知 QML。
    void setConnectionStatus(const QString &status);
    //! 解析遥测帧或命令确认帧并更新内部状态。
    bool processTelemetryFrame(const QByteArray &frame);
    //! 从 JSON 对象读取并校验一个有限数值字段。
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
