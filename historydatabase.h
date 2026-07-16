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
    explicit HistoryDatabase(Backend *backend,
                             SensorBackend *sensorBackend,
                             QObject *parent = nullptr);
    ~HistoryDatabase() override;

    QString databasePath() const;
    QString currentSessionId() const;

    Q_INVOKABLE bool startSession(const QString &username);
    Q_INVOKABLE QVariantList getHistoryRecords() const;
    Q_INVOKABLE QVariantList getSessionData(const QString &sessionId,
                                            const QString &source) const;
    Q_INVOKABLE QVariantList getAbnormalData(const QString &sessionId,
                                             const QString &source) const;

signals:
    void sessionChanged();
    void historyChanged();

private slots:
    void recordPhoneGps();
    void recordPhoneSensor();
    void recordStm32Telemetry();

private:
    bool openDatabase();
    bool createTables();
    bool insertPhoneSnapshot(const QString &eventType);
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
