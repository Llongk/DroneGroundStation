#ifndef HISTORYDATABASE_H
#define HISTORYDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class Backend;
class SensorBackend;

class HistoryDatabase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString currentSessionId READ currentSessionId NOTIFY sessionChanged)

public:
    //! 初始化可迁移 SQLite 数据库并连接手机与 STM32 数据信号。
    explicit HistoryDatabase(Backend *backend,
                             SensorBackend *sensorBackend,
                             QObject *parent = nullptr);
    //! 关闭数据库连接并释放 Qt SQL 连接名。
    ~HistoryDatabase() override;

    //! 返回当前实际使用的数据库文件路径。
    QString databasePath() const;
    //! 返回当前登录会话的唯一标识。
    QString currentSessionId() const;

    //! 为登录用户创建新的历史记录会话。
    Q_INVOKABLE bool startSession(const QString &username);
    //! 返回手机和 STM32 的历史会话摘要。
    Q_INVOKABLE QVariantList getHistoryRecords() const;
    //! 返回指定来源和会话的完整采样数据。
    Q_INVOKABLE QVariantList getSessionData(const QString &sessionId,
                                            const QString &source) const;
    //! 根据阈值提取指定会话中的异常采样点。
    Q_INVOKABLE QVariantList getAbnormalData(const QString &sessionId,
                                             const QString &source) const;

signals:
    //! 登录会话创建成功。
    void sessionChanged();
    //! 手机或 STM32 历史记录已写入。
    void historyChanged();

private slots:
    //! GPS 更新时记录一条手机快照。
    void recordPhoneGps();
    //! 温湿度更新时记录一条手机快照。
    void recordPhoneSensor();
    //! 有效 STM32 遥测更新时写入遥测表。
    void recordStm32Telemetry();

private:
    //! 打开 SQLite 并设置 WAL、同步与外键选项。
    bool openDatabase();
    //! 创建手机、STM32 数据表及查询索引。
    bool createTables();
    //! 将当前手机状态按事件类型写入数据库。
    bool insertPhoneSnapshot(const QString &eventType);
    //! 将经纬度格式化为历史页面显示文本。
    static QString formatPosition(double latitude, double longitude);

    Backend *m_backend = nullptr;
    SensorBackend *m_sensorBackend = nullptr;
    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QString m_sessionId;
    QString m_username;
};

#endif
